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
**  Implementation file server_main.cpp
**
**  Server main driver code.
**
*/
#include "server.hpp"

//  init: main thread
//  read: not used
// write: not uses
//
// these are simply mirrored so the thread quit
// semantics when running standalone are satisfied
volatile bool serverActive;
volatile bool req_serverQuit;

int main(int argc, char** argv)
{
    extern void protocol_main_init();
    protocol_main_init();

    // so matches pattern when standalone/mt
    serverActive=true;
    req_serverQuit=false;

    argset args(argc,argv);
    int rv;
    rv=server_main(&args);

    return rv;
}

