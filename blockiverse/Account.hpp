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
#include "lua-5.3.0/lua_all.h"
#include "bvgame/core.hpp"

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
        s64 playerId;
        lua_State *asUser;
    protected:
    public:
        Account(bvnet::session &sess,serverRoot *server,s64 who) :
            bvnet::object(sess),
            root(*server),
            db(server->get_db()),
            userId(who),
            asUser(luaL_newstate()) {

            /** TODO:
            *   Some of the standard lualibs are not going to behave well
            *   (like the os time stuff) in multithreaded (shared) context.
            *
            *   I'd like to enable as much of it as possible however
            *   and just override the problem implementations with
            *   patched implementations that fix any issues.
            *
            *   (eg: luaB_print which isn't using (UN)LOCK_COUT
            *    it needs an override as it is not sufficient to
            *    merely redefine the print macros in luaconf.h
            *    since the argument loop in luaB_print will
            *    execute across locking cycles thus potentially
            *    have multiargument output being interleaved with
            *    output from other threads.)
            */
            luaL_openlibs(asUser);

            // attempts login
            // throws if multiple login attempt for same account)
            statement doLogin=db.prepare(bvquery::loginAccount);
            db.bind(doLogin,1,userId);
            query_result dontcare;
            dontcare=db.loop_run(doLogin);

            playerId=bvgame::core::getPlayer(db,userId);

            LOCK_COUT
            cout << "Account [" << this
                 << "] ctor (userid=" << userId
                 << ", playerId=" << playerId
                 << ")" << endl;
            UNLOCK_COUT

        }
        virtual ~Account() {
            LOCK_COUT
            cout << "Account [" << this << "] dtor" << endl;
            UNLOCK_COUT

            lua_close(asUser);

            // logout user
            // gobble exceptions since this is a dtor
            try {
                statement doLogout=db.prepare(bvquery::logoutAccount);
                db.bind(doLogout,1,userId);
                query_result dontcare;
                dontcare=db.loop_run(doLogout);
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
