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
**  Declaration (header) file {Thing}.hpp
**
**  {WhatThingDoes}
**
*/
#ifndef BVGAME_CORE_HPP_INCLUDED
#define BVGAME_CORE_HPP_INCLUDED

#include "../database.hpp"

namespace bvgame {

    using bvdb::SQLiteDB;
    using bvdb::DBIsBusy;
    using bvdb::DBError;
    typedef SQLiteDB::statement statement;
    typedef SQLiteDB::query_result query_result;

    namespace core {

        void init(SQLiteDB&);
        s64 getPlayer(SQLiteDB&,s64);

    }

}

#endif // BVGAME_CORE_HPP_INCLUDED
