/**
    Copyright 2016-2019 Red Anchor Trading Co. Ltd.

    Distributed under the Boost Software License, Version 1.0.
    See <http://www.boost.org/LICENSE_1_0.txt>
 */


#include <fostgres/fostgres.hpp>
#include <fostgres/matcher.hpp>

#include <fost/log>
#include <fost/string>


namespace {
    fostlib::nullable<fostgres::match>
            check(const fostlib::json &conf, const fostlib::split_type &parts) {
        if (not conf.has_key("path")) return fostlib::null;
        const fostlib::json &o = conf["path"];
        if (o.size() == parts.size()) {
            fostgres::match m{conf};
            for (std::size_t index{0}; index < o.size(); ++index) {
                auto s = fostlib::coerce<fostlib::string>(o[index]);
                if (s.starts_with("/")) {
                    if (parts[index]
                        != (static_cast<f5::u8view>(s)).substr(1)) {
                        return fostlib::null;
                    }
                } else {
                    const auto n = fostlib::coerce<unsigned int>(o[index]);
                    if (n > 0) {
                        // We have a numeric match so this path part needs to
                        // be exposed as argument number n. This entails
                        // placing it at position (n-1) in the arguments array
                        if (n > m.arguments.size()) { m.arguments.resize(n); }
                        m.arguments[n - 1] = fostlib::coerce<fostlib::string>(
                                fostlib::url::filepath_string(parts[index]));
                    } else {
                        throw fostlib::exceptions::not_implemented(
                                __FUNCTION__,
                                "Path arguments numbers cannot be zero");
                    }
                }
            }
            return m;
        }
        fostlib::log::debug(fostgres::c_fostgres)("", "Path size mismatch")(
                "o", "size", o.size())("parts", "size", parts.size());
        return fostlib::null;
    }
}


fostlib::nullable<fostgres::match> fostgres::matcher(
        const fostlib::json &configuration, const fostlib::string &path) {
    auto parts = fostlib::split(path, "/");
    if (configuration.isobject()) {
        return check(configuration, parts);
    } else if (configuration.isarray()) {
        for (auto conf : configuration) {
            auto m = check(conf, parts);
            if (m) return m;
        }
    }
    return fostlib::null;
}
