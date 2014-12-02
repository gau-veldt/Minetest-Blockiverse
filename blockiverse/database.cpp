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
**  Implementation file database.cpp
**
**  database access to Blockiverse data
**
*/

#include "database.hpp"

namespace bvdb {

    std::string SQLiteDB::file;

    void init_db(std::string where) {
        /**
        * @brief Creates database/tables for blockiverse
        * @param where pathname to database file
        */
        bool sql3_safe=sqlite3_threadsafe();
        LOCK_COUT
        std::cout << "[DB] Database file: " << where << std::endl
                  << "         threading: " << (sql3_safe?"yes":"no") << std::endl;
        UNLOCK_COUT
        if (!sql3_safe) {
            throw NotThreadable("SQLite3 compiled single-thread-only.");
        }
        SQLiteDB::init(where);
    }

};
