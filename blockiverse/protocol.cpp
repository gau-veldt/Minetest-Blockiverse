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
**  Implementation file protocol.cpp
**
**  Implements the protocol for server/client comms
**
*/

#include "protocol.hpp"

int log2_tbl[]={0,1,2,2,3,3,3,3}; /* log2(x) */
int pow2_tbl[]={1,2,4,8};         /* 2^x */

boost::mutex cout_mutex;

/** @var bvnet::reg_object_softmax @brief Maximum size of object registry */
u32 bvnet::reg_objects_softmax=1000;

typedef bvnet::type_map::value_type mappedType;

/** @var bvnet::typeMap @brief Used by serialization to encode or decode protocol valuetypes to actual data types */
bvnet::type_map bvnet::typeMap;

void protocol_main_init() {
    bvnet::typeMap.insert(mappedType(typeid(s64                ).name(),bvnet::vtInt));
    bvnet::typeMap.insert(mappedType(typeid(float              ).name(),bvnet::vtFloat));
    //bvnet::typeMap.insert(mappedType(typeid(string       ).name(),bvnet::vtBlob));
    bvnet::typeMap.insert(mappedType(typeid(string       ).name(),bvnet::vtString));
    bvnet::typeMap.insert(mappedType(typeid(bvnet::obref      ).name(),bvnet::vtObref));
    bvnet::typeMap.insert(mappedType(typeid(bvnet::ob_is_gone ).name(),bvnet::vtDeath));
    bvnet::typeMap.insert(mappedType(typeid(bvnet::method_call).name(),bvnet::vtMethod));
}
