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
        dbValue(char *s) {
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
    typedef std::map<std::string,dbValue::ptr> rowResult;

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
            if (db!=NULL) {
                sqlite3_close(db);
                db=NULL;
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
