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

    const char* queryHasModule=
        "SELECT "
            "moduleId "
        "FROM "
            "Modules "
        "WHERE "
            "name=?1";
    const char* queryAddModule=
        "INSERT "
            "INTO Modules "
                "(name,description) "
            "VALUES "
                "(?1,?2)";

    string queryDelRsvd(const char *tbl,const char *col) {
        string s;
        s="DELETE FROM ";
        s+=tbl;
        s+=" WHERE ";
        s+=col;
        s+="=?2 AND NOT ownerMod=?1";
        return s;
    }
    string queryChkRsvd(const char *tbl,const char *col) {
        string s;
        s="SELECT * FROM ";
        s+=tbl;
        s+=" WHERE ownerMod=?1 AND ";
        s+=col;
        s+="=?2";
        return s;
    }
    string queryAddRsvd(const char *tbl,const char *col) {
        string s;
        s="INSERT INTO ";
        s+=tbl;
        s+=" (ownerMod,";
        s+=col;
        s+=") VALUES (?1,?2)";
        return s;
    }

    namespace core {

        string coreModule="core";
        string coreDesc="Main internal Blockiverse game module.";

        void init(SQLiteDB &db) {
            statement stmtHasModule;
            statement stmtAddModule;
            s64 coreId=-1;
            {
                reread_HasCore:
                stmtHasModule=db.prepare(queryHasModule);
                db.bind(stmtHasModule,1,coreModule);
                query_result rsltHasCore;
                rsltHasCore=db.loop_run(stmtHasModule);
                if (rsltHasCore->size()>0) {
                    coreId=db.get_result<s64>(rsltHasCore,0,0);
                } else {
                    stmtAddModule=db.prepare(queryAddModule);
                    db.bind(stmtAddModule,1,coreModule);
                    db.bind(stmtAddModule,2,coreDesc);
                    query_result rsltAddCore;
                    rsltAddCore=db.loop_run(stmtAddModule);
                    goto reread_HasCore;
                }
            }{
                statement stmtDelRsvd;
                statement stmtChkRsvd;
                statement stmtAddRsvd;
                query_result chkRslt;
                const char* rsvd[]={"Vehicular","Orbital","Falling"};
                for (auto i : rsvd) {
                    stmtDelRsvd=db.prepare(queryDelRsvd("PivotType","pivotType"));
                    db.bind(stmtDelRsvd,1,coreId);
                    db.bind(stmtDelRsvd,2,i);
                    db.loop_run(stmtDelRsvd);

                    stmtChkRsvd=db.prepare(queryChkRsvd("PivotType","pivotType"));
                    db.bind(stmtChkRsvd,1,coreId);
                    db.bind(stmtChkRsvd,2,i);
                    chkRslt=db.loop_run(stmtChkRsvd);

                    if (chkRslt->size()==0) {
                        stmtAddRsvd=db.prepare(queryAddRsvd("PivotType","pivotType"));
                        db.bind(stmtAddRsvd,1,coreId);
                        db.bind(stmtAddRsvd,2,i);
                        db.loop_run(stmtAddRsvd);
                    }
                }
            }

        }

    }

}
