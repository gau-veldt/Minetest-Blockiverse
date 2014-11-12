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

boost::mutex cout_mutex;

u32 bvnet::reg_objects_softmax=1000;

typedef bvnet::type_map::value_type mappedType;
bvnet::type_map bvnet::typeMap;

void protocol_main_init() {
    bvnet::typeMap.insert(mappedType(typeid(s64              ).name(),bvnet::vtInt));
    bvnet::typeMap.insert(mappedType(typeid(float            ).name(),bvnet::vtFloat));
    bvnet::typeMap.insert(mappedType(typeid(std::string      ).name(),bvnet::vtBlob));
    bvnet::typeMap.insert(mappedType(typeid(std::string      ).name(),bvnet::vtString));
    bvnet::typeMap.insert(mappedType(typeid(bvnet::obref     ).name(),bvnet::vtObref));
    bvnet::typeMap.insert(mappedType(typeid(bvnet::ob_is_gone).name(),bvnet::vtDeath));
}
