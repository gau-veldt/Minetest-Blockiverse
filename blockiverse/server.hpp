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
#include "sha1.hpp"

extern volatile bool serverActive;
extern volatile bool req_serverQuit;

//#ifdef BV_SERVER_IMPLEMENTATION
/*
** server internal header only visible to server implementation
*/

#include "RSA/rsa.h"

class serverRoot : public bvnet::object {
private:
    BigInt cli_pub_mod;
    BigInt cli_pub_exp;
    Key *clientKey;
    bool clientValid;
    std::string challenge;
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
            vqueue.push(std::string(getType()));
            break;
        case 1: /* LoginClient */
            {
                /*  in: string: client pubkey modulus
                **      string: client pubkey exponent
                **
                ** out: string: challenge (encrpyted w/pubkey)
                */
                std::string sMod,sExp,eChal;
                sExp=ctx.getarg<std::string>(); /* LIFO is exponent */
                cli_pub_exp=BigInt(sExp);
                sMod=ctx.getarg<std::string>();
                cli_pub_mod=BigInt(sMod);
                clientKey=new Key(cli_pub_mod,cli_pub_exp);
                challenge="TODO: Random Challenge Strings";
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
                std::string hChal,answer;
                answer=ctx.getarg<std::string>();
                SHA1 sha;
                sha.addBytes(challenge.c_str(),challenge.size());
                unsigned char *dig=sha.getDigest();
                std::ostringstream ss;
                for (int i=0;i<20;++i)
                    ss << std::setfill('0') << std::setw(2) << std::hex
                       << (unsigned int)dig[i];
                hChal=ss.str();
                free(dig);
                LOCK_COUT
                std::cout << "[server] Client answered challenge:"  << std::endl
                          << "           mine: " << hChal           << std::endl
                          << "         client: " << answer          << std::endl
                          << "           same: " << (hChal==answer) << std::endl;
                UNLOCK_COUT
                authOk=(hChal==answer);
                vqueue.push(authOk);
                if (!authOk) {
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
