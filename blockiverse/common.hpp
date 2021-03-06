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
**  Declaration (header) file common.hpp
**
**  Declarations for stuff common to client and server.
**
*/
#ifndef BV_COMMON_HPP_INCLUDED
#define BV_COMMON_HPP_INCLUDED

#include "irrTypes.h"
#include "rsa/RSA.h"
#include <boost/thread/mutex.hpp>
#include <string>
#include <exception>
#include <iostream>
#include <iomanip>

using std::string;
using std::cout;
using std::endl;
using std::exception;

// quick and dirty synchronized ostream
extern boost::mutex cout_mutex;
class scoped_cout_lock {
    std::ios state;
public:
    scoped_cout_lock() : state(NULL) {
        cout_mutex.lock();
        state.copyfmt(cout);
    }
    virtual ~scoped_cout_lock() {
        cout.flush();
        cout.copyfmt(state);
        cout_mutex.unlock();
    }
};
#define LOCK_COUT {scoped_cout_lock __scl;
#define UNLOCK_COUT }

typedef unsigned long long u64;
typedef long long s64;
typedef irr::u32 u32;

/*
** meters per parsec
*/
extern u64 m_per_parsec;

struct argset {
    int    c;
    char** v;
    argset(int argc,char **argv)
        : c(argc),v(argv) {}
};

/** @brief ABCs for visitor pattern
 *  Abstact base classes for the visitor pattern */
class IVisitor;
class IVisitable;
class IVisitor {
protected:
    IVisitor()=default;
    ~IVisitor()=default;
public:
    virtual void visit(IVisitable *)=0;
};
class IVisitable {
protected:
    IVisitable()=default;
    ~IVisitable()=default;
public:
    virtual void accept(IVisitor &v) {v.visit(this);}
};

extern int widen(std::wstring &,const std::string &);

#endif // BV_COMMON_HPP_INCLUDED
