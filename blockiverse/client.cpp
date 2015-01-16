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
**  Implementation file client.cpp
**
**  Client implementation
**
*/
#include "client.hpp"

namespace bvclient {

    /** @brief generate visible mesh for visible blocks
        Cenetered on 0,0,0 of loaded block cache looking
        in the direction of the specified viewer angle.
        360deg = 0deg = north, 90deg = east,
        180deg = south, 270deg = west
        @param angle: viewer angle
    */
    void ClientFrontEnd::TriangulateScene(float angle) {
        // normalize angle
        while (angle>=360.0) angle-=360.0;
        while (angle<0.0) angle+=360.0;
        /** @todo
            for third person we look from
            behind player from a fixed camera point
            or nearest obstruction along line from
            player's head to camera point, whichever
            is shorter */

        // get block at 0,0,1 (player head)
        //
        // If this is an obstructing (solid) block show only this block's
        // edge faces and we are done.  minetest et al tend to use the
        // algorithm "treat blocks of same general type (any stone type,
        // any sand type, etc) as the block at player's head as air and
        // show bordering blocks that differ" When head is inside a solid
        // block this creates an "x-ray" mode.  Instead I only show the
        // block the player's head is inside of.
        //
        // If block is a non-obstructing block (air, water, lava, space, etc)
        // the do the bordering blocks algorithm to find surfaces of visible
        // blocks within viewing range (if the loader is optimized any nonvisible
        // blocks behind solid blocks won't be loaded anyways).  Above paragraph
        // explains the bordering blocks algorithm.
        //
        // x-ray may be made into a boolean setting that enables x-ray
        // viewing (performs bordering blocks alg) when center contains
        // a solid block.
        //
    }

}
