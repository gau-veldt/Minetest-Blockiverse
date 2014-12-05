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
**  Declaration (header) file server.hpp
**
**  declarations for server
**
*/
#ifndef BV_SERVER_HPP_INCLUDED
#define BV_SERVER_HPP_INCLUDED

#include "common.hpp"
#include <windows.h>
#include "protocol.hpp"
#include "database.hpp"
#include "sha1.hpp"
#include <boost/nondet_random.hpp>

extern volatile bool serverActive;
extern volatile bool req_serverQuit;
extern boost::random::random_device entropy;

#include "RSA/rsa.h"

using bvdb::SQLiteDB;
using bvdb::DBIsBusy;
using bvdb::DBError;
typedef SQLiteDB::statement statement;
typedef SQLiteDB::query_result query_result;
typedef bvnet::value_queue value_queue;

class Account : public bvnet::object {
private:
    s64 userId;
protected:
public:
    Account(bvnet::session &sess,s64 who)
        : bvnet::object(sess),userId(who) {
        LOCK_COUT
        cout << "Account [" << this << "] ctor (num="
             << userId << ")" << endl;
        UNLOCK_COUT
    }
    virtual ~Account() {
        LOCK_COUT
        cout << "Account [" << this << "] dtor" << endl;
        UNLOCK_COUT
    }
    virtual const char *getType() {return "userAccount";}
};

class serverRoot : public bvnet::object {
private:
    BigInt cli_pub_mod;
    BigInt cli_pub_exp;
    Key *clientKey;
    bool clientValid;
    string challenge;
    SQLiteDB db;
    unsigned int randbits[8];
protected:
    void dmc_LoginClient(value_queue &vqueue);
    void dmc_AnswerChallenge(value_queue &vqueue);
    void dmc_GetAccount(value_queue &vqueue);
public:
    serverRoot(bvnet::session &sess)
        : bvnet::object(sess) {
        register_dmc("LoginClient"      ,(dmc)&serverRoot::dmc_LoginClient);
        register_dmc("AnswerChallenge"  ,(dmc)&serverRoot::dmc_AnswerChallenge);
        register_dmc("GetAccount"       ,(dmc)&serverRoot::dmc_GetAccount);
        clientValid=false;
        challenge="";
        clientKey=NULL;
    }
    virtual ~serverRoot() {
        if (clientKey!=NULL)
            delete clientKey;
    }

    virtual const char *getType() {return "serverRoot";}

};

DWORD WINAPI server_main(LPVOID argvoid);

#endif // BV_SERVER_HPP_INCLUDED
