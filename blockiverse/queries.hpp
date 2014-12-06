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
**  Declaration (header) file queries.hpp
**
**  Blockiverse Database query declrations
**
*/
#ifndef BV_QUERIES_HPP_INCLUDED
#define BV_QUERIES_HPP_INCLUDED

#include <string>
#include <vector>

namespace bvquery {

    /** @brief Iterable for initializing all tables via loop */
    extern std::vector<const char *> init_tables;

    /** @brief Search for account owned by specified client (pubkey) */
    extern const char *findOwner;
    /** @brief Search for account using specified username */
    extern const char *findUser;
    /** @brief Search for whitelist entry allowing access via specified client (pubkey) */
    extern const char *findAllowed;

};

#endif // BV_QUERIES_HPP_INCLUDED
