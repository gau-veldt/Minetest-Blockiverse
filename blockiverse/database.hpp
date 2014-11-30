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
**  Declaration (header) file database.hpp
**
**  Database services (via sqlite)
**
*/
#ifndef BV_DATABASE_HPP_INCLUDED
#define BV_DATABASE_HPP_INCLUDED

#include <string>
#include "common.hpp"
#include "sqlite/sqlite3.h"

extern void init_db(std::string);

class SQLiteDB : private boost::noncopyable {
private:

public:
    SQLiteDB(std::string);
    virtual ~SQLiteDB();
};

#endif // BV_DATABASE_HPP_INCLUDED
