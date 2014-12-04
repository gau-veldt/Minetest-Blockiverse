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

//#ifdef BV_SERVER_IMPLEMENTATION
/*
** server internal header only visible to server implementation
*/

#include "RSA/rsa.h"

using bvdb::SQLiteDB;
using bvdb::DBIsBusy;
using bvdb::DBError;
typedef SQLiteDB::statement statement;
typedef SQLiteDB::query_result query_result;

class Account : public bvnet::object {
private:
public:
    Account(bvnet::session &sess)
        : bvnet::object(sess) {}
    virtual ~Account() {}
    virtual const char *getType() {return "userAccount";}
    virtual void methodCall(unsigned int method) {
        bvnet::scoped_lock lock(ctx.getMutex());
        bvnet::value_queue &vqueue=ctx.getSendQueue();
        switch(method) {
        case 0: /* GetType */
            /* emit object type to output queue as string */
            vqueue.push(string(getType()));
            break;
        }
    }
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
public:
    serverRoot(bvnet::session &sess)
        : bvnet::object(sess) {
        clientValid=false;
        challenge="";
        clientKey=NULL;
    }
    virtual ~serverRoot() {
        if (clientKey!=NULL)
            delete clientKey;
    }

    virtual const char *getType() {return "serverRoot";}
    virtual void methodCall(unsigned int method) {
        bvnet::scoped_lock lock(ctx.getMutex());
        bvnet::value_queue &vqueue=ctx.getSendQueue();
        switch(method) {
        case 0: /* GetType */
            /* emit object type to output queue as string */
            vqueue.push(string(getType()));
            break;
        case 1: /* LoginClient */
            {
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
            break;
        case 2: /* AnswerChallenge */
            {
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
            break;
        case 3: /* GetAccount */
            {
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
                    statement findOwner=db.prepare("SELECT * FROM Owner WHERE userkey=?1");
                    db.bind(findOwner,1,key.str());
                    query_result rsltOwner;
                    try_again=false;
                    do {
                        try {
                            rsltOwner=db.run(findOwner);
                        } catch (DBIsBusy &busy) {
                            try_again=true;
                        }
                    } while (try_again);
                    if (rsltOwner->size()>0)
                        IdOfOwner=db.get_result<s64>(rsltOwner,0,0/* userid */);

                    // see if account exists for this username
                    statement findUser=db.prepare("SELECT * FROM Owner WHERE username=?1");
                    db.bind(findUser,1,user);
                    query_result rsltUser;
                    try_again=false;
                    do {
                        try {
                            rsltUser=db.run(findUser);
                        } catch (DBIsBusy &busy) {
                            try_again=true;
                        }
                    } while (try_again);
                    if (rsltUser->size()>0)
                        IdOfUsername=db.get_result<s64>(rsltUser,0,0/* userid */);

                    if ((IdOfUsername<0)&&(IdOfUsername<0)) {
                        // client has no account and username is unused
                        // action: create it (on success login succeeds)
                        LOCK_COUT
                        cout << "[server] TODO: Create account for "
                             << user << " on pubkey " << key.str() << endl;
                        UNLOCK_COUT
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

                            /* Don't uncomment until an Account object
                            ** is created and its objref put on the output queue */
                            //authOK=true;
                        } else {
                            if (IdOfUsername>=0) {
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
                                statement findAllowed=db.prepare(
                                    "SELECT "
                                        "* "
                                    "FROM "
                                        "AllowedClient "
                                    "WHERE "
                                        "userid=?1 "
                                        "AND allowkey=?2 "
                                        "AND passwd=?3");
                                db.bind(findAllowed,1,IdOfUsername);
                                db.bind(findAllowed,2,key.str());
                                db.bind(findAllowed,3,hPass.str());
                                query_result rsltAllowed;
                                try_again=false;
                                do {
                                    try {
                                        rsltUser=db.run(findUser);
                                    } catch (DBIsBusy &busy) {
                                        try_again=true;
                                    }
                                } while (try_again);
                                if (rsltUser->size()>0) {
                                    // a result row indicates whitelist had
                                    // an allowance entry for this client's pubkey
                                    // and that the passwords matched up
                                    LOCK_COUT
                                    cout << "[server] TODO: Login succeeds for "
                                         << user << "(" << IdOfUsername
                                         << ") on pubkey " << key.str()
                                         << " (via whitelist)" << endl;
                                    UNLOCK_COUT

                                    /* Don't uncomment until an Account object
                                    ** is created and its objref put on the output queue */
                                    //authOK=true
                                }
                            } else {
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

                    if (authOK) {
                        // login successful spawn Account
                        // object and return its objectref
                    } else {
                        // authentication failure
                        vqueue.push(s64(1));
                    }
                } else {
                    // invalid (unauthorized) client
                    vqueue.push(s64(0));
                    ctx.disconnect();
                }
            }
            break;
        }
    }
};

//#else
/*
** make server startup visible only
*/

DWORD WINAPI server_main(LPVOID argvoid);

//#endif // BV_SERVER_IMPLEMENTATION
#endif // BV_SERVER_HPP_INCLUDED
