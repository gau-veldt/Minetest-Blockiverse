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
#define BV_SERVER_IMPLEMENTATION
#include "server.hpp"
#include "settings.hpp"
#include "database.hpp"
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>

boost::random::random_device entropy;

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
    Configurator server_config((cwd/"server.cfg").string());
    server_default_config(server_config);
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
