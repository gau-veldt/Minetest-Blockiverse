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
**  Declaration (header) file client.hpp
**
**  Client defs
**
*/
#ifndef BV_CLIENT_HPP_INCLUDED
#define BV_CLIENT_HPP_INCLUDED

#include "common.hpp"
#include "protocol.hpp"

class clientRoot : public bvnet::object {
public:
    clientRoot(bvnet::session &sess)
        : bvnet::object(sess) {
    }
    virtual ~clientRoot() {}

    virtual const char *getType() {return "clientRoot";}
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

#endif // BV_CLIENT_HPP_INCLUDED
