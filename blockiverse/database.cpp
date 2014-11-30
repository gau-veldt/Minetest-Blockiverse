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

void init_db(std::string where) {
    /*
    ** Creates database/tables for blockiverse
    */
    LOCK_COUT
    std::cout << "Database file: " << where << std::endl;
    UNLOCK_COUT
}
