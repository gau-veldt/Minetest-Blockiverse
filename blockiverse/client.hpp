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
private:
protected:
public:
    clientRoot(bvnet::session &sess)
        : bvnet::object(sess) {
    }
    virtual ~clientRoot() {
        LOCK_COUT
        cout << "clientRoot [" << this << "] gone (via session " << &ctx << ')' << endl;
        UNLOCK_COUT
    }

    virtual const char *getType() {return "clientRoot";}
};

#endif // BV_CLIENT_HPP_INCLUDED
