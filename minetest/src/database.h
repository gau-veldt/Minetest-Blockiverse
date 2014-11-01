/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef DATABASE_HEADER
#define DATABASE_HEADER

#include <list>
#include <string>
#include "irr_v3d.h"
#include "irrlichttypes.h"

#ifndef PP
	#define PP(x) "("<<(x).X<<","<<(x).Y<<","<<(x).Z<<")"
#endif

class Database
{
public:
	virtual void beginSave() = 0;
	virtual void endSave() = 0;

    /*
	** for blockiverse engine
	*/

	/*
	** gets all sparse objects in range of center
	** listed objects will be translated such that
	** (0,0,0) corresponds to center
	**
	** because maximum range is much smaller than s128
	** the translation step converts the large 128-bit
	** integer absolute positions into much smaller
	** engine-friendly types suitable for rendering
	**
	** @PARAM
	**        dst: reference to an (empty) list to output to
	**     center: center point of the search
	**      range: search radius (in meters)
	**
	** NB: You want to be able to look up objects in a rather large range
	**     even if chunkoids themselves are not actually meshified beyond
	**     a much smaller range
	**
	/*
	//virtual void objectsInRange(std::list<v3s32> &dst, v3s128 center, u32 range) = 0;
	/* list blocks comprising specified chunkoid (ships, asteroids, etc) */
	//virtual void listChunkoid(std::list<v3s16> &dst, u64 id) = 0;

    // for minetest engine
	virtual bool saveBlock(v3s16 blockpos, std::string &data) = 0;
	virtual std::string loadBlock(v3s16 blockpos) = 0;
	s64 getBlockAsInteger(const v3s16 pos) const;
	v3s16 getIntegerAsBlock(s64 i) const;
	virtual void listAllLoadableBlocks(std::list<v3s16> &dst) = 0;

	virtual int Initialized(void)=0;
	virtual ~Database() {};
};
#endif
