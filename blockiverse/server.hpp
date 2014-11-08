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

#ifdef BV_SERVER_IMPLEMENTATION
/*
** server internal header only visible to server implementation
*/

class serverRoot : public bvnet::object {
public:
    serverRoot(bvnet::session &sess)
        : bvnet::object(sess) {
    }
    virtual ~serverRoot() {}
    const char *getType() {return "ServerRoot_0_000_00";}
    void methodCall(int method) {
        bvnet::value_queue &vqueue=ctx.getSendQueue();
        switch(method) {
        case 0:
            vqueue.push(std::string(getType()));
            break;
        }
    }
};

#else
/*
** make server startup visible only
*/

DWORD WINAPI server_main(LPVOID argvoid);

#endif // BV_SERVER_IMPLEMENTATION
#endif // BV_SERVER_HPP_INCLUDED
