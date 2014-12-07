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
**  Implementation file server.cpp
**
**  Server implementation
**
*/
#include "auto/version.h"
#include "common.hpp"
#include <windows.h>
#include <iostream>
#include <map>
#include "sqlite/sqlite3.h"
#include "protocol.hpp"
#include "server.hpp"
#include "settings.hpp"
#include "database.hpp"
#include "queries.hpp"
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>

#include "Account.hpp"

boost::random::random_device entropy;

using bv::Account;

void server_default_config(Configurator &cfg) {
    cfg["port"]="37001";
}

struct context {
    tcp::socket *socket;
    bvnet::session *session;
    bvnet::object *root;
};

class context_manager {
private:
    context *ctx;
public:
    context_manager(context *raii_ctx) : ctx(raii_ctx) {}
    ~context_manager() {
        ctx->session->clear_rooted_objects();
        delete ctx->root;
        ctx->root=NULL;
        delete ctx->session;
        ctx->session=NULL;
        ctx->socket->close();
        delete ctx->socket;
        ctx->socket=NULL;
        delete ctx;
        ctx=NULL;
    }
};

DWORD WINAPI server_boot(LPVOID lpvCtx) {
    /*
    ** entry point for server slave connected to client
    */
    context &ctx=*((context*)lpvCtx);   // make a proper reference
    context_manager raii_ctx(&ctx);     // proper cleanup on thread exit

    LOCK_COUT
    cout << "[server] session [" << ctx.session << "] slave on socket " << ctx.socket << endl;
    UNLOCK_COUT
    //ctx.session->dump(cout);

    ctx.session->bootstrap(ctx.root);

    while (!req_serverQuit && ctx.session->run()) {
        /* until session dies or server shutdown */
    }

    LOCK_COUT
    cout << "[server] slave for socket "<< ctx.socket << " finished." << endl;
    UNLOCK_COUT

    return 0;
}

typedef std::map<boost::thread*,io_service*> s_list;
s_list sessions;

DWORD WINAPI server_main(LPVOID argvoid) {
    int argc=0;
    char **argv=NULL;
    if (argvoid!=NULL) {
        argc=((argset*)argvoid)->c;
        argv=((argset*)argvoid)->v;
    }
    boost::filesystem::path cwd=boost::filesystem::current_path();
    Configurator server_config((cwd/"server.cfg").string(),server_default_config);
    server_config.read_cmdline(argc,argv);

    LOCK_COUT
    cout << "[server] Version is: " << auto_ver << endl;
    cout << "[server] Starting in: " << cwd << endl;
    /*if (argc==0) {
        cout << "[server] Argument passing failed (argc==0)" << endl;
    } else {
        cout << "[server] Argument count: " << argc << endl;
        for (int i=0;i<argc;++i) {
            cout << "[server] Argument " << i << ": " << argv[i] << endl;
        }
    }*/
    UNLOCK_COUT

    bvdb::init_db((cwd/"bv_db").string());
    try {
        SQLiteDB db;    // RAII
        for (auto query : bvquery::init_tables)
            db.runOnce(query);
    } catch (DBError &e) {
        LOCK_COUT
        cout << "[DB] Error creating tables:" << endl
                  << "     " << e.what()       << endl;
        UNLOCK_COUT
    }

    io_service acceptor_io;
    int port=v2int(server_config["port"]);
    LOCK_COUT
    cout << "[server] listening on port " << port
              << " (io=" << &acceptor_io << ")"<< endl;
    UNLOCK_COUT
    tcp::acceptor listener(acceptor_io,tcp::endpoint(tcp::v4(),port));

    while (!req_serverQuit) {
        io_service *s_chld_sess_io=new io_service;
        tcp::socket* new_conn=new tcp::socket(*s_chld_sess_io);
        listener.accept(*new_conn);

        // create context object
        context *ctx=new context;
        // store socket in context
        ctx->socket=new_conn;
        // create new session in context
        ctx->session=new bvnet::session();
        // link connection socket to new session
        ctx->session->set_conn(*new_conn);
        // create session's serverRoot object
        // which is also stored in the context
        ctx->root=new serverRoot(*ctx->session);
        // the worker thread manages the context
        boost::thread *thd=new boost::thread(server_boot,ctx);
        if (thd!=NULL) {
            // to keep track of threads
            sessions[thd]=s_chld_sess_io;
        } else {
            // TODO: error
        }

        /*
        ** Prune any completed session threads
        */
        s_list::iterator sThread=sessions.begin();
        while (sThread!=sessions.end()) {
            if (sThread->first->timed_join(boost::posix_time::seconds(0))) {
                delete sThread->first;  // delete thread
                delete sThread->second; // delete thread's io_service
                sessions.erase(sThread++);
            } else {
                ++sThread;
            }
        }
    }

    // cleanly quit open sessions
    s_list::iterator sThread=sessions.begin();
    while (sThread!=sessions.end()) {
        sThread->second->stop();        // make thread's io_service stop
        sThread->first->join();         // wait for it to quit
        delete sThread->first;          // delete thread
        delete sThread->second;         // delete thread's io_service
        sessions.erase(sThread++);
    }

    LOCK_COUT
    cout << "[server] shutdown complete." << endl;
    UNLOCK_COUT
    serverActive=false;
    return 0;
}

void serverRoot::dmc_LoginClient(value_queue &vqueue) {
    /*  in: string: client pubkey modulus
    **      string: client pubkey exponent
    **
    ** out: string: challenge (encrpyted w/pubkey)
    */
    string sMod,sExp,eChal;
    sExp=ctx.getarg<string>(); /* LIFO is exponent */
    cli_pub_exp=BigInt(sExp);
    sMod=ctx.getarg<string>();
    cli_pub_mod=BigInt(sMod);
    clientKey=new Key(cli_pub_mod,cli_pub_exp);
    for (int i=0;i<8;++i)
        randbits[i]=entropy();
    unsigned char *randbyte=(unsigned char *)randbits;
    challenge.clear();
    /*
    ** Dividing down to valid printables (32-127) makes
    ** the challenge effectively [192..208] bits or 6-6.5
    ** bits per byte. An SHA1 may potentially collide past
    ** 160 bits thus making 160 bits the minimum, a lower
    ** bound ND randomness of 192 bits will suffice.
    **
    ** In release builds where the challenge won't be
    ** displayed dividing down to printables will not
    ** be necessary and the entire 256 bits may be
    ** utilized.
    */
    for (int i=0;i<31;++i) {
        challenge+=(unsigned char)(32+((int)(((float)randbyte[i])/2.68421)));
    }
    eChal=RSA::Encrypt(challenge,*clientKey);
    vqueue.push(eChal);
}

void serverRoot::dmc_AnswerChallenge(value_queue &vqueue) {
    /*
    **  in: string: SHA1 matching decrypted challenge string
    **              (string encrypted with client privkey)
    **
    ** out: integer: 1 if accepted
    **
    ** NB: invalid response drops the connection
    */
    s64 authOk=0;
    string hChal,answer;
    answer=ctx.getarg<string>();
    SHA1 sha;
    sha.addBytes(challenge.c_str(),challenge.size());
    unsigned char *dig=sha.getDigest();
    std::ostringstream ss;
    for (int i=0;i<20;++i)
        ss << std::setfill('0') << std::setw(2) << std::hex
           << (unsigned int)dig[i];
    hChal=ss.str();
    free(dig);
    /*LOCK_COUT
    cout << "[server] Client answered challenge:"  << endl
              << "           mine: " << hChal           << endl
              << "         client: " << answer          << endl
              << "           same: " << (hChal==answer) << endl;
    UNLOCK_COUT*/
    if (hChal==answer) {
        authOk=1;
        clientValid=true;
    }
    vqueue.push(authOk);
    if (!clientValid) {
        ctx.disconnect();
    }
}

void serverRoot::dmc_GetAccount(value_queue &vqueue) {
    /*
    ** Called by authenticated client to
    ** log on to a user account
    **
    ** in: string username
    **     string password
    **
    ** out: objectref account or
    **      int result code
    **
    ** username is the username of the account
    ** to log onto either the username owned by
    ** the client or anotehr user who has linked
    ** (whitelisted) this client
    **
    ** as a special case if client does not currently own a
    ** user account and the specified username is not in use
    ** by another account it will be created and registered
    ** with this client's pubkey as its owner
    **
    ** a pasword string must always be sent but
    ** only used when the username being logged
    ** onto via linked (whitelisted) client
    **
    ** nonempty password are sent encrypted
    ** (by client with privkey) and server
    ** will decode with connected client's
    ** pubkey
    **
    ** returns either the objectref
    ** of the user account on success
    ** or an integer result code on
    ** failure:
    **
    ** 0: invalid client (did not answer chellenge)
    ** 1: unauthorized (not owner of the account or incorrect
    **    password supplied for linked (whitelisted) account
    **
    ** calling this method from unvalidated
    ** client causes disconnection after
    ** returning a result code of 0
    **
    */
    string pass,user;
    // LIFO is password
    pass=RSA::Decrypt(ctx.getarg<string>(),*clientKey);
    user=ctx.getarg<string>();
    LOCK_COUT
    cout << "[server] request login for user " << user << endl;
    //cout << "         password " << pass << endl;
    UNLOCK_COUT

    if (clientValid) {
        std::ostringstream key;
        key << cli_pub_mod << ":" << cli_pub_exp;
        // the client public key exponent in the current RSA lib
        // is always 65537 but future/forked clients might use differing
        // exponents so it needs to be saved.
        bool try_again,authOK=false;
        /*
        ** Access database for existing user account.
        ** Create (login suceeds) if does not exist
        **
        ** If account exists verify client's key is account owner.
        ** If client's key is owner login succeeds.
        **
        ** If not the owner check the whitelist for client's key.
        ** If not there login fails.
        **
        ** If there is an entry verify the provided password against
        ** the whitelist entry.  Login succeeds on password match,
        ** fails otherwise.
        **/
        s64 IdOfOwner=-1;
        s64 IdOfUsername=-1;
        // see if account exists for this owner
        retry_login:
        authOK=false;
        statement findOwner=db.prepare(bvquery::findOwner);
        db.bind(findOwner,1,key.str());
        query_result rsltOwner;
        do {
            try_again=false;
            try {
                rsltOwner=db.run(findOwner);
            } catch (DBIsBusy &busy) {
                try_again=true;
            }
        } while (try_again);
        if (rsltOwner->size()>0)
            IdOfOwner=db.get_result<s64>(rsltOwner,0,bvquery::result::findOwner::userid);

        // see if account exists for this username
        statement findUser=db.prepare(bvquery::findUser);
        db.bind(findUser,1,user);
        query_result rsltUser;
        do {
            try_again=false;
            try {
                rsltUser=db.run(findUser);
            } catch (DBIsBusy &busy) {
                try_again=true;
            }
        } while (try_again);
        if (rsltUser->size()>0)
            IdOfUsername=db.get_result<s64>(rsltUser,0,bvquery::result::findUser::userid);

        LOCK_COUT
        cout << "rsltOwner " << rsltOwner;
        cout << "rsltUser " << rsltUser;
        UNLOCK_COUT

        if ((IdOfOwner<0)&&(IdOfUsername<0)) {
            // client has no account and username is unused
            // action: create it (on success login succeeds) then
            // goto retries the login.  This allows simpler error
            // checking on the INSERT query and allows using the
            // normal autocommit semantics.
            statement createAccount=db.prepare(bvquery::createAccount);
            db.bind(createAccount,1,user);
            db.bind(createAccount,2,key.str());
            query_result dontcare;
            bool insert_ok=false;
            try {
                do {
                    try_again=false;
                    try {
                        dontcare=db.run(createAccount);
                    } catch (DBIsBusy &busy) {
                        try_again=true;
                    }
                } while (try_again);
                insert_ok=true;
            } catch (DBError &e) {
                LOCK_COUT
                cout << "[DB] Failed to create account for " << user
                     << " via " << key.str() << " due to:" << endl
                     << "     " << e.what() << endl;
                UNLOCK_COUT
            }
            if (insert_ok) {
                // retry the login as we don't yet know the auto column's userid
                IdOfOwner=-1;
                IdOfUsername=-1;
                goto retry_login;
            }
            //
            // insert failed (not due to busy)
            //
            // probably a race codition where someone else
            // concurrently created an account on this
            // username between us checking availability
            // and performing the insert so login fails.
            //
            // not worth the hassler of a transaction
            //
            authOK=false;
        } else {
            if (IdOfOwner==IdOfUsername) {
                // userid of owner matches userid of username
                // action: login succeeds
                LOCK_COUT
                cout << "[server] TODO: Login succeeds for "
                     << user << "(" << IdOfOwner
                     << ") on pubkey " << key.str()
                     << " (owner)" << endl;
                UNLOCK_COUT

                try {
                    bvnet::session::shared acct=ctx.get_shared(new Account(ctx,this,IdOfOwner));
                    auto &obLval=*acct;
                    u32 acctId=ctx.getIdOf(&obLval);
                    LOCK_COUT
                    cout << "[server] Account login " << user << " on session "
                         << &ctx <<  " objectid=" << acctId << endl;
                    UNLOCK_COUT
                    vqueue.push(bvnet::obref(acctId));
                    authOK=true;
                } catch (DBError &e) {
                    LOCK_COUT
                    cout << "[server] Account (userid=" << IdOfOwner
                         << ") login failed: " << e.what() << endl;
                    UNLOCK_COUT
                    authOK=false;
                }
            } else {
                if (IdOfUsername>=0) {
                    LOCK_COUT
                    cout << "[server] session " << &(this->ctx) << ": Account "
                         << user << " belongs to someone else." << endl;
                    UNLOCK_COUT
                    // This is the attempt to log in as
                    // an existing username via a client
                    // (pubkey) that does not own the account
                    // specified by the username.
                    //
                    // This case needs an additional query
                    // to see of the owner of the username has
                    // linked (whitelisted/allowed) this client's
                    // pubkey and whitelist entry's password
                    // matches the password supplied.
                    SHA1 sha;
                    sha.addBytes(pass.c_str(),pass.size());
                    unsigned char *dig=sha.getDigest();
                    std::ostringstream hPass;
                    for (int i=0;i<20;++i)
                        hPass << std::setfill('0') << std::setw(2) << std::hex
                              << (unsigned int)dig[i];
                    free(dig);
                    statement findAllowed=db.prepare(bvquery::findAllowed);
                    db.bind(findAllowed,1,IdOfUsername);
                    db.bind(findAllowed,2,key.str());
                    db.bind(findAllowed,3,hPass.str());
                    query_result rsltAllowed;
                    do {
                        try_again=false;
                        try {
                            rsltAllowed=db.run(findUser);
                        } catch (DBIsBusy &busy) {
                            try_again=true;
                        }
                    } while (try_again);
                    if (rsltAllowed->size()>0) {
                        s64 IdOfOtherOwner;
                        IdOfOtherOwner=db.get_result<s64>(rsltAllowed,0,bvquery::result::findAllowed::userid);
                        // a result row indicates whitelist had
                        // an allowance entry for this client's pubkey
                        // and that the passwords matched up
                        /** TODO:
                        *
                        *   Ensure we aren't logging in the same user twice.
                        */
                        LOCK_COUT
                        cout << "[server] TODO: Login succeeds for "
                             << user << "(" << IdOfOtherOwner
                             << ") on pubkey " << key.str()
                             << " (via whitelist)" << endl;
                        UNLOCK_COUT

                        try {
                            bvnet::session::shared acct=ctx.get_shared(new Account(ctx,this,IdOfOtherOwner));
                            auto &obLval=*acct;
                            u32 acctId=ctx.getIdOf(&obLval);
                            LOCK_COUT
                            cout << "[server] Account login " << user << " on session "
                                 << &ctx <<  " objectid=" << acctId << endl;
                            UNLOCK_COUT
                            vqueue.push(bvnet::obref(acctId));
                            authOK=true;
                        } catch (DBError &e) {
                            LOCK_COUT
                            cout << "[server] Account (userid=" << IdOfOtherOwner
                                 << ") login failed: " << e.what() << endl;
                            UNLOCK_COUT
                            authOK=false;
                        }
                    }
                } else {
                    LOCK_COUT
                    cout << "[server] session " << &(this->ctx)
                         << ": attempt to create 2nd account "
                         << user << endl;
                    UNLOCK_COUT
                    // this would be the case of creating
                    // a new account (username not in use)
                    // by a client (pubkey) already posessing
                    // an account but this is prohibited by
                    // the db (only one account per pubkey)
                    // so let the login attempt fail
                    authOK=false;
                }
            }
        }

        if (!authOK) {
            // authentication failure
            vqueue.push(s64(1));
        }
    } else {
        // invalid (unauthorized) client
        vqueue.push(s64(0));
        ctx.disconnect();
    }
}
