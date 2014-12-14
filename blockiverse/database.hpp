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
    class dbValue;
};

std::ostream& operator<<(std::ostream &,const bvdb::dbValue &);

namespace bvdb {

    extern void init_db(string);

    /** @brief Column type casting error */
    struct ColumnCastFailed : public std::runtime_error {
        ColumnCastFailed(const char *msg) : std::runtime_error(msg) {}
    };
    /** @brief We got a single-threaded SQLite library */
    struct NotThreadable : public std::runtime_error {
        NotThreadable(const char *msg) : std::runtime_error(msg) {}
    };
    /** @brief SQLITE_BUSY gets its own football for easier catching */
    struct DBIsBusy : public std::runtime_error {
        DBIsBusy() : std::runtime_error(string("SQLite is busy.")) {}
    };
    /** @brief Errors SQLite and database access errors
    *   Bad parameters, incorrect usage, violated constraints, etc.
    */
    struct DBError : public std::runtime_error {
        enum _flags {noop,release};
        [[noreturn]] static void busy_aware_throw(int code,const char *msg,_flags f=noop) {
            if (code==SQLITE_BUSY) {
                throw DBIsBusy();
            } else {
                throw DBError(msg,f);
            }
        }
        DBError(const char *msg,_flags f=noop) : std::runtime_error(string(msg)) {
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
                    delete (string*)data;
                default:
                    break;
                }
                data=NULL;
                affinity=null;
            }
        }

        friend std::ostream& ::operator<<(std::ostream &,const dbValue &);

        dbValue() {
            data=NULL;
            affinity=null;
        }
        dbValue(const char *s) {
            data=new string(s);
            affinity=text;
        }
        dbValue(const char* src,size_t len) {
            data=new string;
            ((string*)data)->assign(src,len);
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
        operator string() {
            switch (affinity) {
            case integer:
                return std::to_string(*((s64*)data));
            case real:
                return std::to_string(*((double*)data));
            case text:
            case blob:
                return *((string*)data);
            default:
                throw ColumnCastFailed("null -> string");
            }
        }
    };

    /** @brief keys are column names which yield the dbValue ptrs */
    typedef std::map<int,dbValue::ptr> rowResult;
    typedef std::vector<rowResult> Result;
    /** @brief statement cache map type */
    typedef std::map<string,sqlite3_stmt*> stmt_map;

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
        static string file;
        sqlite3 *db;
        /** @brief optimizer cache of statements for faster operations
        *   These will be appropriately released in dtor */
        stmt_map stmtCache;
    public:
        static void init(const string path) {
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
                DBError::busy_aware_throw(rc,sqlite3_errmsg(db));
            }
            runOnce("PRAGMA foreign_keys=ON;");
        }
        virtual ~SQLiteDB() {
            for (auto&& i : stmtCache) {
                LOCK_COUT
                cout << "[DB] delete cached statement " << i.second << endl;
                UNLOCK_COUT
                sqlite3_finalize(i.second);
                i.second=NULL;
            }
            if (db!=NULL) {
                sqlite3_close(db);
                db=NULL;
            }
        }

        typedef std::shared_ptr<Result> query_result;
        template<typename T>
        T get_result(query_result rslt,int row,int col) {return (T)(*(((*rslt)[row])[col]));}

        query_result run(sqlite3_stmt *stmt) {
            query_result data(new Result);
            int rc;
            do {
                rc=sqlite3_step(stmt);
                if (rc==SQLITE_ROW) {
                    data->resize(data->size()+1);
                    rowResult &cur=data->back();
                    int nrows=sqlite3_column_count(stmt);
                    for (int i=0;i<nrows;++i) {
                        s64 int_val;
                        double double_val;
                        const char *data_val;
                        int data_len;
                        switch (sqlite3_column_type(stmt,i)) {
                        case SQLITE_NULL:
                            cur[i]=dbValue::ptr(new dbValue);
                            break;
                        case SQLITE_INTEGER:
                            int_val=sqlite3_column_int(stmt,i);
                            cur[i]=dbValue::ptr(new dbValue(int_val));
                            break;
                        case SQLITE_FLOAT:
                            double_val=sqlite3_column_double(stmt,i);
                            cur[i]=dbValue::ptr(new dbValue(double_val));
                            break;
                        case SQLITE_TEXT:
                            data_val=(const char *)sqlite3_column_text(stmt,i);
                            cur[i]=dbValue::ptr(new dbValue(data_val));
                            break;
                        case SQLITE_BLOB:
                            data_val=(const char *)sqlite3_column_blob(stmt,i);
                            data_len=sqlite3_column_bytes(stmt,i);
                            cur[i]=dbValue::ptr(new dbValue(data_val,data_len));
                            break;
                        }
                    }
                }
            } while (rc==SQLITE_ROW);
            if (rc!=SQLITE_DONE) {
                DBError::busy_aware_throw(rc,sqlite3_errmsg(db));
            }
            LOCK_COUT
            cout << "[DB] " << stmt << ": " << data->size() << " rows returned." << endl;
            UNLOCK_COUT
            return data;
        }

        query_result loop_run(sqlite3_stmt *stmt) {
            bool try_again;
            query_result rc;
            do {
                try_again=false;
                try {
                    rc=run(stmt);
                } catch (DBIsBusy &busy) {
                    try_again=true;
                }
            } while (try_again);
            return rc;
        }


        typedef sqlite3_stmt *statement;
        void bind(statement s,int idx,const string &val) {
            /** @brief bind argument with a string */
            int rc=sqlite3_bind_text(s,idx,val.c_str(),val.size(),SQLITE_TRANSIENT);
            if (rc) {
                DBError::busy_aware_throw(rc,sqlite3_errmsg(db));
            }
            LOCK_COUT
            cout << "[DB] " << s << ": " << "?" << idx << " = \"" << val << '"' << endl;
            UNLOCK_COUT
        }
        void bind(statement s,int idx,const char *val,int len) {
            /** @brief bind argument with binary data (blob) */
            int rc=sqlite3_bind_blob(s,idx,val,len,SQLITE_TRANSIENT);
            if (rc) {
                DBError::busy_aware_throw(rc,sqlite3_errmsg(db));
            }
            LOCK_COUT
            cout << "[DB] " << s << ": " << "?" << idx << " = (blob of size " << len << ")" << endl;
            UNLOCK_COUT
        }
        void bind(statement s,int idx,s64 &val) {
            /** @brief bind argument with an int64 */
            int rc=sqlite3_bind_int64(s,idx,val);
            if (rc) {
                DBError::busy_aware_throw(rc,sqlite3_errmsg(db));
            }
            LOCK_COUT
            cout << "[DB] " << s << ": " << "?" << idx << " = " << val << endl;
            UNLOCK_COUT
        }
        void bind(statement s,int idx,int &val) {
            /** @brief bind argument with an integer */
            int rc=sqlite3_bind_int(s,idx,val);
            if (rc) {
                DBError::busy_aware_throw(rc,sqlite3_errmsg(db));
            }
            LOCK_COUT
            cout << "[DB] " << s << ": " << "?" << idx << " = " << val << endl;
            UNLOCK_COUT
        }
        void bind(statement s,int idx,double &val) {
            /** @brief bind argument with a double */
            int rc=sqlite3_bind_double(s,idx,val);
            if (rc) {
                DBError::busy_aware_throw(rc,sqlite3_errmsg(db));
            }
            LOCK_COUT
            cout << "[DB] " << s << ": " << "?" << idx << " = " << val << endl;
            UNLOCK_COUT
        }
        void bind_null(statement s,int idx) {
            /** @brief bind argument with nothing (null) */
            int rc=sqlite3_bind_null(s,idx);
            if (rc) {
                DBError::busy_aware_throw(rc,sqlite3_errmsg(db));
            }
            LOCK_COUT
            cout << "[DB] " << s << ": " << "?" << idx << " = (null)" << endl;
            UNLOCK_COUT
        }
        sqlite3_stmt* prepare(string sql) {
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
                LOCK_COUT
                cout << "[DB] Statement (cached): "
                          << stmt << ": " << sql << endl;
                UNLOCK_COUT
                return stmt;
            } else {
                sqlite3_stmt *target=NULL;
                int rc=sqlite3_prepare_v2(db,sql.c_str(),sql.size(),&target,NULL);
                if (rc) {
                    DBError::busy_aware_throw(rc,sqlite3_errmsg(db));
                } else {
                    if (target!=NULL) {
                        // only cache if not not NULL
                        stmtCache[sql]=target;
                    }
                    LOCK_COUT
                    cout << "[DB] Statement compiled: "
                              << target << ": " << sql << endl;
                    UNLOCK_COUT
                    return target;
                }
            }
        }

        void runOnce(const string sql) {
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
            if (sql.size()>180) {
                cout << "[DB] runOnce: " << sql.substr(0,177) << "..." << endl;
            } else {
                cout << "[DB] runOnce: " << sql << endl;
            }
            UNLOCK_COUT

            char *err=NULL;
            int rc=sqlite3_exec(db,sql.c_str(),NULL,NULL,&err);
            if (rc) {
                DBError::busy_aware_throw(rc,err,DBError::release);
            }
        }
    };

};

inline std::ostream& operator<<(std::ostream &os,const bvdb::dbValue &dbv) {
    os << "Data<";
    switch (dbv.affinity) {
    case bvdb::dbValue::integer:
        os << "int>="
           << *((s64*)dbv.data);
        break;
    case bvdb::dbValue::real:
        os << "real>="
           << *((double*)dbv.data);
        break;
    case bvdb::dbValue::text:
        os << "text>="
           << *((string*)dbv.data);
        break;
    case bvdb::dbValue::blob:
        os << "blob len="
           << ((string*)dbv.data)->size()
           << ">";
        break;
    default:
        os << "null>";
        break;
    }
    return os;
}

inline std::ostream& operator <<(std::ostream& os,bvdb::SQLiteDB::query_result r) {
    os << "Query Result" << endl;
    int nrow=0;
    for (auto row : *r) {
        os << "    " << ++nrow << ": (";
        bool comma=false;
        for (auto col : row) {
            if (comma)
                os << ',';
            os << *(col.second);
            comma=true;
        }
        os << ")" << endl;
    }
    if (nrow==0)
        os << "    Nothing." << endl;
    return os;
}

#endif // BV_DATABASE_HPP_INCLUDED
