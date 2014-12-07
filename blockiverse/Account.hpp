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
**  Declaration (header) file Account.hpp
**
**  User account (declaration)
**
*/
#ifndef BV_ACCOUNT_HPP_INCLUDED
#define BV_ACCOUNT_HPP_INCLUDED

#include "common.hpp"
#include "protocol.hpp"
#include "database.hpp"
#include "queries.hpp"
#include "server.hpp"

using bvdb::SQLiteDB;
using bvdb::DBIsBusy;
using bvdb::DBError;
typedef SQLiteDB::statement statement;
typedef SQLiteDB::query_result query_result;

namespace bv {

    class Account : public bvnet::object {
    private:
        serverRoot &root;
        SQLiteDB &db;
        s64 userId;
    protected:
    public:
        Account(bvnet::session &sess,serverRoot *server,s64 who) :
            bvnet::object(sess),
            root(*server),
            db(server->get_db()),
            userId(who) {

            LOCK_COUT
            cout << "Account [" << this << "] ctor (userid="
                 << userId << ")" << endl;
            UNLOCK_COUT

            // attempts login
            // throws if multiple login attempt for same account)
            bool try_again;
            statement doLogin=db.prepare(bvquery::loginAccount);
            db.bind(doLogin,1,userId);
            query_result dontcare;
            do {
                try_again=false;
                try {
                    dontcare=db.run(doLogin);
                } catch (DBIsBusy &busy) {
                    try_again=true;
                }
            } while (try_again);
        }
        virtual ~Account() {
            LOCK_COUT
            cout << "Account [" << this << "] dtor" << endl;
            UNLOCK_COUT
            // logout user
            // gobble exceptions since this is a dtor
            try {
                statement doLogout=db.prepare(bvquery::logoutAccount);
                db.bind(doLogout,1,userId);
                query_result dontcare;
                bool try_again;
                do {
                    try_again=false;
                    try {
                        dontcare=db.run(doLogout);
                    } catch (DBIsBusy &e) {
                        try_again=true;
                    }
                } while (try_again);
            } catch (DBError &e) {
                LOCK_COUT
                cout << "Account [" << this << "] dtor: " << e.what() << endl;
                UNLOCK_COUT
            }
        }
        virtual const char *getType() {return "userAccount";}
    };

};

#endif // BV_ACCOUNT_HPP_INCLUDED
