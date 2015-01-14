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
**  Declaration (header) file chunk.hpp
**
**  Declarations for chunks and blocks
**
*/
#ifndef BV_CHUNK_HPP_INCLUDED
#define BV_CHUNK_HPP_INCLUDED

#include <inttypes.h>

namespace bvmap {


    class Node {
    public:
        uint16_t blockId;       // 0=air/space
        uint8_t  blkLight;
        uint8_t  flags;
    };

    class Chunk {
        Node cells[16*16*16];
    };

}

#endif // BV_CHUNK_HPP_INCLUDED
