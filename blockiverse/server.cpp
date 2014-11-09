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
#include "sqlite/sqlite3.h"
#include "protocol.hpp"
#define BV_SERVER_IMPLEMENTATION
#include "server.hpp"
#include "settings.hpp"

struct context {
    tcp::socket *socket;
    bvnet::session *session;
    bvnet::object *root;
};

DWORD WINAPI server_main(LPVOID argvoid) {
    property_map server_config;
    set_config_defaults(server_config);

    int argc=0;
    char **argv=NULL;
    if (argvoid!=NULL) {
        argc=((argset*)argvoid)->c;
        argv=((argset*)argvoid)->v;
    }

    LOCK_COUT
    std::cout << "[server] Version is: " << auto_ver << std::endl;

    if (argc==0) {
        std::cout << "[server] Argument passing failed (argc==0)" << std::endl;
    } else {
        std::cout << "[server] Argument count: " << argc << std::endl;
        for (int i=0;i<argc;++i) {
            std::cout << "[server] Argument " << i << ": " << argv[i] << std::endl;
        }
    }
    UNLOCK_COUT

    io_service io;
    int port=v2int(server_config["port"]);
    LOCK_COUT
    std::cout << "[server] listening on port " << port << std::endl;
    UNLOCK_COUT
    tcp::acceptor listener(io,tcp::endpoint(tcp::v4(),port));

    for (;;) {
        tcp::socket* new_conn=new tcp::socket(io);
        listener.accept(*new_conn);
        // create context object
        // for connection thread
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
        // it's ugly but I'll  need to have the
        // connection's thread handle RAII of
        // the context object and proper deletion
        // of its members
    }

    return 0;
}
