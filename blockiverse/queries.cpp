/*
**
**  Minetest-Blockiverse
**
**  Incorporates portions of code from minetest 0.4.10-dev
**
**  Blockiverse
**  Copyright (C) 2014 Brian Jack <gau_veldt@hotmail.com>
**  Distributed as free software using the copyleft
**  LGPL Version 3 license:
**  https://www.gnu.org/licenses/lgpl-3.0.en.html
**  See file LICENSE in ../
**
**  Implementation file queries.hpp
**
**  Blockiverse Database query definitions
**
*/
#include "queries.hpp"

#include <initializer_list>

namespace bvquery {

    /** @brief Iterable for initializing all tables via loop */
    std::vector<const char *> init_tables={
        /** @brief Init client ownership table */
        "CREATE TABLE IF NOT EXISTS Owner ("
            "userid INTEGER PRIMARY KEY ASC NOT NULL"
            ",username TEXT UNIQUE NOT NULL"
            ",userkey TEXT UNIQUE NOT NULL"
        ")"/* WITHOUT ROWID*/,

        /** @brief Init client whitelist table */
        "CREATE TABLE IF NOT EXISTS AllowedClient ("
            "userid INTEGER NOT NULL REFERENCES Owner (userid) ON DELETE CASCADE"
            ",allowkey TEXT NOT NULL"
            ",passwd TEXT UNIQUE NOT NULL"
            ",PRIMARY KEY (userid,allowkey)"
        ")"
    };

    /** @brief Search for account owned by specified client (pubkey) */
    const char *findOwner=
        "SELECT "
            "* "
        "FROM "
            "Owner "
        "WHERE "
            "userkey=?1";

    /** @brief Search for account using specified username */
    const char *findUser=
        "SELECT "
            "* "
        "FROM "
            "Owner "
        "WHERE "
            "username=?1";

    /** @brief Search for whitelist entry allowing access via specified client (pubkey) */
    const char *findAllowed=
        "SELECT "
            "* "
        "FROM "
            "AllowedClient "
        "WHERE "
            "userid=?1 "
            "AND allowkey=?2 "
            "AND passwd=?3";

    /** @brief Create new account for specified username and client (pubkey) */
    const char *createAccount=
        "INSERT "
            "INTO Owner "
                "(username,userkey) "
            "VALUES "
                "(?1,?2)";

};
