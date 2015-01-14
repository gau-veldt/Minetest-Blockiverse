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
        /** @brief decoration above this block (0 if none) */
        uint16_t deco_above_block;
        /** @brief decoration below this block (0 if none) */
        uint16_t deco_below_block;
        /** @brief decoration in-front-of this block (0 if none) */
        uint16_t deco_infrontof_block;
        /** @brief decoration behind this block (0 if none) */
        uint16_t deco_behind_block;
        /** @brief decoration to left of this block (0 if none) */
        uint16_t deco_leftof_block;
        /** @brief decoration to right of this block (0 if none) */
        uint16_t deco_rightof_block;
    };

    class Chunk {
        Node cells[16*16*16];
    };

}

#endif // BV_CHUNK_HPP_INCLUDED
