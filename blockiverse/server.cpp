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

DWORD WINAPI server_main(LPVOID argvoid) {
    int argc=0;
    char **argv=NULL;
    if (argvoid!=NULL) {
        argc=((argset*)argvoid)->c;
        argv=((argset*)argvoid)->v;
    }

    std::cout << "[server] Version is: " << auto_ver << std::endl;

    property_map config;
    set_config_defaults(config);

    if (argc==0) {
        std::cout << "[server] Argument passing failed (argc==0)" << std::endl;
    } else {
        std::cout << "[server] Argument count: " << argc << std::endl;
        for (int i=0;i<argc;++i) {
            std::cout << "[server] Argument " << i << ": " << argv[i] << std::endl;
        }
    }

    io_service io;
    int port=v2int(config["port"]);
    std::cout << "[server] listening on port " << port << std::endl;
    tcp::acceptor listener(io,tcp::endpoint(tcp::v4(),port));

    for (;;) {
        tcp::socket* new_conn=new tcp::socket(io);
        listener.accept(*new_conn);
        // create session
        bvnet::session *new_sess=new bvnet::session();
        new_sess->set_conn(*new_conn);
        // create session's server root object
        serverRoot *sroot=new serverRoot(*new_sess);
    }

    return 0;
}
