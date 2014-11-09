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
std::ios cout_save_st(NULL);

u32 bvnet::reg_objects_softmax=1000;
