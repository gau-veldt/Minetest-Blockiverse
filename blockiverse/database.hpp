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

#include <memory>
#include <exception>
#include <string>
#include "common.hpp"
#include <boost/core/noncopyable.hpp>
#include "sqlite/sqlite3.h"

namespace bvdb {

    extern void init_db(std::string);

    struct ColumnCastFailed : public std::runtime_error {
        ColumnCastFailed(const char *msg) : std::runtime_error(msg) {}
    };
    struct NotThreadable : public std::runtime_error {
        NotThreadable(const char *msg) : std::runtime_error(msg) {}
    };
    struct DBError : public std::runtime_error {
        enum _flags {noop,release};
        DBError(const char *msg,_flags f=noop) : std::runtime_error(std::string(msg)) {
            if (f==release) sqlite3_free((void*)msg);
        }
    };

    class dbValue : boost::noncopyable {
    public:
        typedef std::shared_ptr<dbValue> ptr;
        enum aff {null,integer,real,text,blob};
    private:
        void *data;
        aff affinity;
    public:
        aff type() {return affinity;}

        ~dbValue() {
            if (data!=NULL) {
                switch (affinity) {
                case integer:
                    delete (s64*)data;
                    break;
                case real:
                    delete (double*)data;
                    break;
                case text:
                case blob:
                    delete (std::string*)data;
                default:
                    break;
                }
                data=NULL;
                affinity=null;
            }
        }

        dbValue() {
            data=NULL;
            affinity=null;
        }
        dbValue(const char *s) {
            data=new std::string(s);
            affinity=text;
        }
        dbValue(const char* src,size_t len) {
            data=new std::string;
            ((std::string*)data)->assign(src,len);
            affinity=blob;
        }
        dbValue(const double &r) {
            data=new double(r);
            affinity=real;
        }
        dbValue(const s64 &z) {
            data=new s64(z);
            affinity=integer;
        }


        operator s64() {
            switch (affinity) {
            case integer:
                return *((s64*)data);
            case real:
                return (s64)(*((double*)data));
            default:
                throw ColumnCastFailed("null/text/blob -> int");
            }
        }
        operator double() {
            switch (affinity) {
            case integer:
                return (double)(*((s64*)data));
            case real:
                return *((double*)data);
            default:
                throw ColumnCastFailed("null/text/blob -> real");
            }
        }
        operator std::string() {
            switch (affinity) {
            case integer:
                return std::to_string(*((s64*)data));
            case real:
                return std::to_string(*((double*)data));
            case text:
            case blob:
                return *((std::string*)data);
            default:
                throw ColumnCastFailed("null -> string");
            }
        }
    };

    /** @brief keys are column names which yield the dbValue ptrs */
    typedef std::map<int,dbValue::ptr> rowResult;
    typedef std::vector<rowResult> wholeResult;
    /** @brief statement cache map type */
    typedef std::map<std::string,sqlite3_stmt*> stmt_map;

    /**
     *  @brief Manager class for SQLite3 database and connections
     *
     *  Manages specification of database file, creating connections
     *  and shutting down.
     *
     *  Cacheing system utilizes prepared statements to remember SQL
     *  statements on first use to prevent the overhead of subsequent
     *  compilations of SQL statements so long as class user utilizes
     *  binding semantics versus inline parameters.
     */
    class SQLiteDB : private boost::noncopyable {
    private:
        static std::string file;
        sqlite3 *db;
        /** @brief optimizer cache of statements for faster operations
        *   These will be appropriately released in dtor */
        stmt_map stmtCache;
    public:
        static void init(const std::string path) {
            if (file.size()==0)
                file=path;
            else
                throw DBError("Multiple attempts to set database file.");
        }
        SQLiteDB() {
            db=NULL;
            if (file.size()==0) {
                throw DBError("Database file not set.");
            }
            int rc=sqlite3_open(file.c_str(),&db);
            if (rc) {
                throw DBError(sqlite3_errmsg(db));
            }
        }
        virtual ~SQLiteDB() {
            for (auto&& i : stmtCache) {
                sqlite3_finalize(i.second);
                i.second=NULL;
            }
            if (db!=NULL) {
                sqlite3_close(db);
                db=NULL;
            }
        }

        std::shared_ptr<wholeResult> run(sqlite3_stmt *stmt) {
            std::shared_ptr<wholeResult> data(new wholeResult);
            int rc;
            do {
                rc=sqlite3_step(stmt);
                if (rc==SQLITE_ROW) {
                    data->resize(data->size()+1);
                    rowResult cur=data->back();
                    for (int i=0;i<sqlite3_column_count(stmt);++i) {
                        switch (sqlite3_column_type(stmt,i)) {
                        case SQLITE_NULL:
                            cur[i]=dbValue::ptr(new dbValue);
                            break;
                        case SQLITE_INTEGER:
                            cur[i]=dbValue::ptr(new dbValue((s64)sqlite3_column_int(stmt,i)));
                            break;
                        case SQLITE_FLOAT:
                            cur[i]=dbValue::ptr(new dbValue((double)sqlite3_column_double(stmt,i)));
                            break;
                        case SQLITE_TEXT:
                            cur[i]=dbValue::ptr(new dbValue((const char *)sqlite3_column_text(stmt,i)));
                            break;
                        case SQLITE_BLOB:
                            cur[i]=dbValue::ptr(
                                new dbValue(
                                    (const char *)sqlite3_column_text(stmt,i),
                                    sqlite3_column_bytes(stmt,i)
                                ));
                            break;
                        }
                    }
                }
            } while (rc==SQLITE_ROW);
            if (rc!=SQLITE_DONE) {
                throw DBError(sqlite3_errmsg(db));
            }
            return data;
        }

        sqlite3_stmt* prepare(std::string sql) {
            /** @brief compile sql to prepared statement
            *   Results will be cached so compiles aren't repeated.
            *   Statement will be reset on cache hit.
            */
            stmt_map::iterator lookup=stmtCache.find(sql);
            if (lookup != stmtCache.end()) {
                // we have it so don't recompile
                // we'll be nice and reset the statemnet
                // since the prepare operation implies it
                sqlite3_stmt *stmt=lookup->second;
                sqlite3_reset(stmt);
                sqlite3_clear_bindings(stmt);
                return stmt;
            } else {
                sqlite3_stmt *target=NULL;
                int rc=sqlite3_prepare_v2(db,sql.c_str(),sql.size(),&target,NULL);
                if (rc) {
                    throw DBError(sqlite3_errmsg(db));
                } else {
                    if (target!=NULL) {
                        // only cache if not not NULL
                        stmtCache[sql]=target;
                    }
                    return target;
                }
            }
        }

        void runOnce(const std::string sql) {
            /**
            *   @brief Run SQL commend once
            *
            *   Runs an SQL intneded to be used only once (eg: table initializer)
            *   thus no cacheing is performed of the SQL statement (ie: prepared
            *   statements).
            *
            *   Furthermore assumes no results.
            *
            *   @param sql SQL statement to execute
            *   @throw DBError should execution fail
            */
            LOCK_COUT
            std::cout << "[DB] runOnce SQL: " << sql << std::endl;
            UNLOCK_COUT

            char *err=NULL;
            int rc=sqlite3_exec(db,sql.c_str(),NULL,NULL,&err);
            if (rc)
                throw DBError(err,DBError::release);
        }
    };

};

#endif // BV_DATABASE_HPP_INCLUDED
