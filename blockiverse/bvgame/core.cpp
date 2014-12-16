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
    typedef std::pair<SQLType::_type,string> propSlot;
    typedef std::vector<propSlot> propList;
    typedef std::map<string,s64> enumList;

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
    string queryEnumRsvd(const char *tbl) {
        string s;
        s="SELECT * FROM ";
        s+=tbl;
        s+=" WHERE ownerMod=?1";
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

    string queryChkProp=
        "SELECT * "
            "FROM "
                "Property "
            "WHERE "
                "ownerMod=?1 AND name=?2";

    string queryAddProp=
        "INSERT INTO "
            "Property "
                "(ownerMod,type,name) "
            "VALUES "
                "(?1,?2,?3)";

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
    void enumRsvd(SQLiteDB &db,s64 ownerId,const char* tbl, const rsvdList &rsvd, enumList &eList) {
        statement stmtEnumRsvd;
        query_result enumRslt;
        s64 slotId;
        string slotName;
        stmtEnumRsvd=db.prepare(queryEnumRsvd(tbl));
        db.bind(stmtEnumRsvd,1,ownerId);
        enumRslt=db.loop_run(stmtEnumRsvd);
        for (size_t i=0;i<enumRslt->size();++i) {
            slotId=db.get_result<s64>(enumRslt,i,0);
            slotName=db.get_result<string>(enumRslt,i,2);
            eList[slotName]=slotId;
            LOCK_COUT
            cout << tbl << "[\"" << slotName << "\"]=" << slotId << endl;
            UNLOCK_COUT
        }
    }

    void initProp(SQLiteDB &db,s64 ownerId,const propList &props) {
        statement stmtChkProp;
        statement stmtAddProp;
        query_result chkRslt;
        for (auto i : props) {

            stmtChkProp=db.prepare(queryChkProp);
            db.bind(stmtChkProp,1,ownerId);
            db.bind(stmtChkProp,2,i.second);
            chkRslt=db.loop_run(stmtChkProp);

            if (chkRslt->size()==0) {
                s64 pType=i.first;
                stmtAddProp=db.prepare(queryAddProp);
                db.bind(stmtAddProp,1,ownerId);
                db.bind(stmtAddProp,2,pType);
                db.bind(stmtAddProp,3,i.second);
                db.loop_run(stmtAddProp);
            }
        }
    }

    namespace core {

        enumList PivotType;
        enumList EntityType;

        const char* queryCreateEntity=
            "INSERT INTO "
                "Entity "
                    "(entityType,objectId) "
                "VALUES "
                    "(?1,?2)";

        string queryFindEntity(s64 entType) {
            string s="SELECT * FROM Entity WHERE entityType=";
            s+=std::to_string(entType);
            s+=" AND objectId=?1";
            return s;
        }

        void init(SQLiteDB &db) {
            s64 coreId=initModule(db,
                "core",
                "Main internal Blockiverse game module.");
            rsvdList rsvd;

            rsvd=rsvdList({"Vehicular","Orbital","Falling"});
            initRsvd(db,coreId,"PivotType","pivotType",rsvd);
            enumRsvd(db,coreId,"PivotType",rsvd,PivotType);

            rsvd=rsvdList({"Star","Planet","Chunkoid","Vehicle","Player","Mob"});
            initRsvd(db,coreId,"EntityType","entityType",rsvd);
            enumRsvd(db,coreId,"EntityType",rsvd,EntityType);

            propList props;
            //props.push_back(propSlot(SQLType::integer,"HP"));
            //props.push_back(propSlot(SQLType::integer,"MaxHP"));
            initProp(db,coreId,props);
        }

        s64 getPlayer(SQLiteDB &db,s64 acctId) {
            /** @brief Get player linked to user.
            *   Create one if it doesn't exist.
            *   @param db Database handle
            *   @param acctId user account id
            *   @return id of player object */
            while (true) {
                statement stmt=db.prepare(queryFindEntity(EntityType["Player"]));
                db.bind(stmt,1,acctId);
                query_result rsltFindPlayer=db.loop_run(stmt);
                if (rsltFindPlayer->size()>0) {
                    return db.get_result<s64>(rsltFindPlayer,0,0);
                }
                // Create player object
                stmt=db.prepare(queryCreateEntity);
                db.bind(stmt,1,EntityType["Player"]);
                db.bind(stmt,2,acctId);
                db.loop_run(stmt);
            }
        }

    }

}
