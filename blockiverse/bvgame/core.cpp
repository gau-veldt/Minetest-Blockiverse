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
**  Blockiverse Game Implementation file core.cpp
**
**  Basic core blockiverse game and database initializers.
**
*/

#include "../common.hpp"
#include "core.hpp"

namespace bvgame {

    const char* queryHasModule="SELECT moduleId FROM Modules WHERE name=?1";
    const char* queryAddModule="INSERT INTO Modules (name,description) VALUES (?1,?2)";


    namespace core {

        string coreModule="core";
        string coreDesc="Main internal Blockiverse game module.";

        void init(SQLiteDB &db) {
            statement stmtHasModule;
            statement stmtAddModule;
            bool try_again;
            s64 coreId=-1;

            {
                reread_HasCore:
                stmtHasModule=db.prepare(queryHasModule);
                db.bind(stmtHasModule,1,coreModule);
                query_result rsltHasCore;
                do {
                    try_again=false;
                    try {
                        rsltHasCore=db.run(stmtHasModule);
                    } catch (DBIsBusy &busy) {
                        try_again=true;
                    }
                } while (try_again);
                if (rsltHasCore->size()>0) {
                    coreId=db.get_result<s64>(rsltHasCore,0,0);
                } else {
                    stmtAddModule=db.prepare(queryAddModule);
                    db.bind(stmtAddModule,1,coreModule);
                    db.bind(stmtAddModule,2,coreDesc);
                    query_result rsltAddCore;
                    do {
                        try_again=false;
                        try {
                            rsltAddCore=db.run(stmtAddModule);
                        } catch (DBIsBusy &busy) {
                            try_again=true;
                        }
                    } while (try_again);
                    goto reread_HasCore;
                }
            }
            LOCK_COUT
            cout << "[bvgame] core: module id is " << coreId << endl;
            UNLOCK_COUT
        }

    }

}
