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
**  Declaration (header) file Account.hpp
**
**  User account (declaration)
**
*/
#ifndef BV_ACCOUNT_HPP_INCLUDED
#define BV_ACCOUNT_HPP_INCLUDED

#include "common.hpp"
#include "protocol.hpp"

namespace bv {

    class Account : public bvnet::object {
    private:
        s64 userId;
    protected:
    public:
        Account(bvnet::session &sess,s64 who)
            : bvnet::object(sess),userId(who) {
            LOCK_COUT
            cout << "Account [" << this << "] ctor (userid="
                 << userId << ")" << endl;
            UNLOCK_COUT
        }
        virtual ~Account() {
            LOCK_COUT
            cout << "Account [" << this << "] dtor" << endl;
            UNLOCK_COUT
        }
        virtual const char *getType() {return "userAccount";}
    };

};

#endif // BV_ACCOUNT_HPP_INCLUDED
