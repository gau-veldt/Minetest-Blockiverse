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

#include "auto/version.h"
#include "irrTypes.h"
#include "rsa/RSA.h"

typedef unsigned long long u64;
typedef long long s64;
typedef irr::u32 u32;

/*
** meters per parsec
*/
extern u64 m_per_parsec;

#endif // BV_COMMON_HPP_INCLUDED
