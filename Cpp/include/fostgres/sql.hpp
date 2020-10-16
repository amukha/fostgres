/**
    Copyright 2016-2020 Red Anchor Trading Co. Ltd.

    Distributed under the Boost Software License, Version 1.0.
    See <http://www.boost.org/LICENSE_1_0.txt>
 */


#pragma once


#include <fostgres/iteration.hpp>
#include <fost/urlhandler>


namespace fostgres {


    struct match;


    /// Return a database connection configuration. Normally just use
    /// the variant that returns the connection directly unless you need
    /// to reconfigure the connection configuration first.
    fostlib::json connection_config(
            fostlib::json config, const fostlib::http::server::request &req);

    /// Return a database connection after taking into account any
    /// connection configuration that may be in the request. This is the
    /// preferred method for making a database connection
    fostlib::pg::connection connection(
            fostlib::json config, const fostlib::http::server::request &req);
    /// Return a database connection using the configuration exactly as
    /// provided
    fostlib::pg::connection connection(
            fostlib::json config,
            const fostlib::nullable<fostlib::string> &,
            const fostlib::http::server::request &req);

    /// Execute the command and return the column names and data
    std::pair<std::vector<fostlib::string>, fostlib::pg::recordset>
            sql(fostlib::pg::connection &, const fostlib::string &cmd);
    /// Execute the command with arguments and return the column names and data
    std::pair<std::vector<fostlib::string>, fostlib::pg::recordset>
            sql(fostlib::pg::connection &,
                const fostlib::string &cmd,
                const std::vector<fostlib::string> &args);
    /// Execute the command with arguments and return the column names and data
    std::pair<std::vector<fostlib::string>, fostlib::pg::recordset>
            sql(fostlib::pg::connection &,
                const fostlib::string &cmd,
                const std::vector<fostlib::json> &args);

    /// Return the data associated with a SELECT configuration (e.g. a GET)
    std::pair<std::vector<fostlib::string>, fostlib::pg::recordset> select_data(
            fostlib::pg::connection &,
            const fostlib::json &select,
            const fostgres::match &m,
            const fostlib::http::server::request &req);


}
