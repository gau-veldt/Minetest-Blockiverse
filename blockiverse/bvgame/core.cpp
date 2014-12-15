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
#include <vector>

namespace bvgame {

    struct SQLType {
        typedef enum {
            integer=SQLITE_INTEGER,
            real=SQLITE_FLOAT,
            text=SQLITE_TEXT,
            blob=SQLITE_BLOB
        } _type;
    };

    typedef std::vector<string> rsvdList;

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

    s64 initModule(SQLiteDB &db,const string moduleName,const string moduleDesc) {
        statement stmtHasModule;
        statement stmtAddModule;
        s64 modId=-1;
        while (modId==-1) {
            stmtHasModule=db.prepare(queryHasModule);
            db.bind(stmtHasModule,1,moduleName);
            query_result rsltHasModule;
            rsltHasModule=db.loop_run(stmtHasModule);
            if (rsltHasModule->size()>0) {
                modId=db.get_result<s64>(rsltHasModule,0,0);
            } else {
                stmtAddModule=db.prepare(queryAddModule);
                db.bind(stmtAddModule,1,moduleName);
                db.bind(stmtAddModule,2,moduleDesc);
                db.loop_run(stmtAddModule);
            }
        }
        return modId;
    }


    void initRsvd(SQLiteDB &db,s64 ownerId,const char* tbl,const char* col, const rsvdList &rsvd) {
        statement stmtDelRsvd;
        statement stmtChkRsvd;
        statement stmtAddRsvd;
        query_result chkRslt;
        for (auto i : rsvd) {
            stmtDelRsvd=db.prepare(queryDelRsvd(tbl,col));
            db.bind(stmtDelRsvd,1,ownerId);
            db.bind(stmtDelRsvd,2,i);
            db.loop_run(stmtDelRsvd);

            stmtChkRsvd=db.prepare(queryChkRsvd(tbl,col));
            db.bind(stmtChkRsvd,1,ownerId);
            db.bind(stmtChkRsvd,2,i);
            chkRslt=db.loop_run(stmtChkRsvd);

            if (chkRslt->size()==0) {
                stmtAddRsvd=db.prepare(queryAddRsvd(tbl,col));
                db.bind(stmtAddRsvd,1,ownerId);
                db.bind(stmtAddRsvd,2,i);
                db.loop_run(stmtAddRsvd);
            }
        }
    }

    namespace core {

        void init(SQLiteDB &db) {
            s64 coreId=initModule(db,
                "core",
                "Main internal Blockiverse game module.");
            rsvdList rsvd;

            rsvd=rsvdList({"Vehicular","Orbital","Falling"});
            initRsvd(db,coreId,"PivotType","pivotType",rsvd);

            rsvd=rsvdList({"Star","Planet","Chunkoid","Vehicle","Player","Mob"});
            initRsvd(db,coreId,"EntityType","entityType",rsvd);
        }

    }

}
