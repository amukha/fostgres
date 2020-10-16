/**
    Copyright 2016-2020 Red Anchor Trading Co. Ltd.

    Distributed under the Boost Software License, Version 1.0.
    See <http://www.boost.org/LICENSE_1_0.txt>
 */


#include "updater.hpp"

#include <fostgres/fostgres.hpp>

#include <fost/csj.parser.hpp>
#include <fost/json>
#include <fost/log>
#include <fost/parse/json.hpp>
#include <fost/push_back>


namespace {


    const fostgres::responder c_csj("csj", fostgres::response_csj);


    void csv_string(std::string &into, const fostlib::string &str) {
        if (str.find_first_of("\"\n,") != fostlib::string::npos) {
            into += '"';
            for (auto ch : str.memory()) {
                switch (ch) {
                case '"': into += ch; // Double up this one
                default: into += ch;
                }
            }
            into += '"';
        } else {
            into += static_cast<std::string_view>(str);
        }
    }

    struct csj_mime : public fostlib::mime {
        enum class output { csj, csv } format;
        mutable bool done = false;
        mutable std::vector<fostlib::string> columns;
        mutable fostlib::pg::recordset rs;

        struct csj_iterator : public fostlib::mime::iterator_implementation {
            const csj_mime::output format;
            fostlib::pg::recordset rs;
            fostlib::pg::recordset::const_iterator iter, end;
            std::string current;
            bool sent_first = false;

            csj_iterator(
                    csj_mime::output format,
                    std::vector<fostlib::string> &&columns,
                    fostlib::pg::recordset &&r)
            : format(format),
              rs(std::move(r)),
              iter(rs.begin()),
              end(rs.end()) {
                current.reserve(64 * 1024);
                for (std::size_t index{0}; index < columns.size(); ++index) {
                    if (index) current += ',';
                    switch (format) {
                    case csj_mime::output::csj:
                        fostlib::json::unparse(current, columns[index]);
                        break;
                    case csj_mime::output::csv:
                        csv_string(current, columns[index]);
                        break;
                    }
                }
                current += '\n';
                if (iter != end) line();
            }

            void line() {
                while (iter != end && current.length() < 48 * 1024) {
                    auto record = *iter;
                    for (std::size_t index{0}; index < record.size(); ++index) {
                        if (index) current += ',';
                        switch (format) {
                        case csj_mime::output::csj:
                            fostlib::json::unparse(
                                    current, record[index], false);
                            break;
                        case csj_mime::output::csv:
                            if (record[index].isnull()) {
                                // Do nothing -- null is an empty entry
                            } else if (not fostlib::coerce<
                                               fostlib::nullable<f5::u8view>>(
                                               record[index])) {
                                csv_string(
                                        current,
                                        fostlib::json::unparse(
                                                record[index], false));
                            } else {
                                csv_string(
                                        current,
                                        fostlib::coerce<f5::u8view>(
                                                record[index]));
                            }
                            break;
                        }
                    }
                    current += '\n';
                    ++iter;
                }
            }

            fostlib::const_memory_block operator()() {
                if (!sent_first) {
                    sent_first = true;
                    return fostlib::const_memory_block(
                            current.c_str(),
                            current.c_str() + current.length());
                } else if (iter == end) {
                    return fostlib::const_memory_block();
                } else {
                    current.clear();
                    line();
                    return fostlib::const_memory_block(
                            current.c_str(),
                            current.c_str() + current.length());
                }
            }
        };

        f5::u8view mime_type(fostlib::string const &accept) {
            const auto csj_pos = accept.find("application/csj");
            const auto csv_pos = accept.find("application/csv");
            if (csj_pos < csv_pos) {
                return "application/csj";
            } else if (csv_pos < csj_pos) {
                return "application/csv";
            } else {
                return "text/plain";
            }
        }

        csj_mime(
                fostlib::string const &accept,
                std::vector<fostlib::string> &&cols,
                fostlib::pg::recordset &&rs)
        : mime(fostlib::mime::mime_headers(), mime_type(accept)),
          format(output::csj),
          columns(std::move(cols)),
          rs(std::move(rs)) {
            if (headers()["Content-Type"].value() == "application/csv") {
                format = output::csv;
            }
        }

        std::unique_ptr<iterator_implementation> iterator() const {
            if (done) {
                throw fostlib::exceptions::not_implemented(
                        __func__, "The data can only be iterated over once");
            }
            done = true;
            return std::unique_ptr<iterator_implementation>(new csj_iterator(
                    format, std::move(columns), std::move(rs)));
        }

        bool boundary_is_ok(const fostlib::string &) const {
            throw fostlib::exceptions::not_implemented(__func__);
        }
        std::ostream &print_on(std::ostream &) const {
            throw fostlib::exceptions::not_implemented(__func__);
        }
    };


    std::pair<boost::shared_ptr<fostlib::mime>, int>
            get(fostlib::pg::connection &cnx,
                fostlib::json const &config,
                fostgres::match const &m,
                fostlib::http::server::request &req) {
        auto data = fostgres::select_data(cnx, m.configuration["GET"], m, req);
        return std::make_pair(
                boost::shared_ptr<fostlib::mime>(new csj_mime(
                        req.headers()["Accept"].value(), std::move(data.first),
                        std::move(data.second))),
                200);
    }


    std::pair<boost::shared_ptr<fostlib::mime>, int>
            patch(fostlib::pg::connection &cnx,
                  fostlib::json const &config,
                  fostgres::match const &m,
                  fostlib::http::server::request &req) {
        auto logger = fostlib::log::debug(fostgres::c_fostgres);
        logger("", "CSJ PATCH");
        fostlib::json work_done{fostlib::json::object_t()};
        std::size_t records{};

        // We're going to need these items later
        fostgres::updater handler{m.configuration["PATCH"], cnx, m, req};

        // Interpret body as UTF8 and split into lines. Ensure it's not empty
        fostlib::csj::parser data(f5::u8view(req.data()->data()));
        logger("header", data.header());

        // Parse each line and send it to the database
        for (auto line(data.begin()), e(data.end()); line != e; ++line) {
            std::pair<boost::shared_ptr<fostlib::mime>, int> error;
            auto linejson = line.as_json();
            auto linedata = handler.data(linejson);
            switch (handler.perform()) {
            case fostgres::updater::action::do_default: [[fallthrough]];
            case fostgres::updater::action::insertable:
                error = handler.insert(linejson, linedata, records);
                break;
            case fostgres::updater::action::updateable:
                error = handler.update(linejson, linedata, records);
                break;
            }
            if (error.first) return error;
            ++records;
        }
        cnx.commit();
        fostlib::insert(work_done, "records", records);
        boost::shared_ptr<fostlib::mime> response(new fostlib::text_body(
                fostlib::json::unparse(work_done, true),
                fostlib::mime::mime_headers(), "application/json"));
        return std::make_pair(response, 200);
    }


    std::pair<boost::shared_ptr<fostlib::mime>, int>
            put(fostlib::pg::connection &cnx,
                fostlib::json const &config,
                fostgres::match const &m,
                fostlib::http::server::request &req) {
        auto logger = fostlib::log::debug(fostgres::c_fostgres);
        logger("", "CSJ PUT");

        fostgres::updater handler{m.configuration["PUT"], cnx, m, req};
        fostlib::json work_done{fostlib::json::object_t()};

        auto const select_sql =
                fostlib::coerce<f5::u8view>(m.configuration["PUT"]["existing"]);
        fostgres::put_records_seen dbkeys{cnx, select_sql, m, req};
        fostlib::insert(work_done, "selected", dbkeys.size());
        logger("selected", dbkeys.size());

        // Process the incoming data and put it into the database. Also
        // record the keys seen
        {
            // Interpret body as UTF8 and split into lines. Ensure it's not empty
            fostlib::csj::parser data(f5::u8view(req.data()->data()));
            logger("header", data.header());
            std::size_t records{};

            // Parse each line and send it to the database
            for (auto line(data.begin()), e(data.end()); line != e; ++line) {
                auto [error, inserted] =
                        handler.upsert(line.as_json(), records);
                if (error.first) return error;
                ++records;
                dbkeys.record(inserted.first);
            }
            fostlib::insert(work_done, "records", records);
            logger("records", records);
        }

        // Look through the initial keys to find any that weren't in the
        // incoming data so the rows can be deleted
        {
            auto const sql = fostlib::coerce<f5::u8view>(
                    m.configuration["PUT"]["delete"]);
            std::size_t const deleted = dbkeys.delete_left_over_records(sql);
            fostlib::insert(work_done, "deleted", deleted);
            logger("deleted", deleted);
        }

        cnx.commit();

        boost::shared_ptr<fostlib::mime> response(new fostlib::text_body(
                fostlib::json::unparse(work_done, true),
                fostlib::mime::mime_headers(), "application/json"));
        return std::make_pair(response, 200);
    }


    std::pair<boost::shared_ptr<fostlib::mime>, int>
            del(fostlib::pg::connection &cnx,
                fostlib::json const &config,
                fostgres::match const &m,
                fostlib::http::server::request &req) {
        auto sql = fostlib::coerce<fostlib::string>(m.configuration["DELETE"]);
        auto sp = cnx.procedure(fostlib::coerce<fostlib::utf8_string>(sql));
        sp.exec(m.arguments);
        cnx.commit();
        boost::shared_ptr<fostlib::mime> response(new fostlib::text_body(""));
        return std::make_pair(response, 200);
    }


}


std::pair<boost::shared_ptr<fostlib::mime>, int> fostgres::response_csj(
        fostlib::pg::connection &cnx,
        fostlib::json const &config,
        fostgres::match const &m,
        fostlib::http::server::request &req) {
    if (req.method() == "GET") {
        return get(cnx, config, m, req);
    } else if (req.method() == "PATCH") {
        return patch(cnx, config, m, req);
    } else if (req.method() == "PUT") {
        return put(cnx, config, m, req);
    } else if (req.method() == "POST") {
        return put(cnx, config, m, req);
    } else if (req.method() == "DELETE") {
        return del(cnx, config, m, req);
    } else {
        throw fostlib::exceptions::not_implemented(
                __FUNCTION__, "Must use GET, HEAD, DELETE, PUT or PATCH");
    }
}
