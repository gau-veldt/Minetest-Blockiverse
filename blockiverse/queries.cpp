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
**  Implementation file queries.hpp
**
**  Blockiverse Database query definitions
**
*/
#include "queries.hpp"

#include <initializer_list>

namespace bvquery {

    /** @brief Iterable for initializing all tables via loop */
    std::vector<const char *> init_tables={
        /** @brief Init client ownership table */
        "CREATE TABLE IF NOT EXISTS Owner ("
            "userid INTEGER PRIMARY KEY ASC NOT NULL"
            ",username TEXT UNIQUE NOT NULL"
            ",userkey TEXT UNIQUE NOT NULL"
        ")",

        /** @brief Init client whitelist table */
        "CREATE TABLE IF NOT EXISTS AllowedClient ("
            "userid INTEGER NOT NULL REFERENCES Owner (userid) ON DELETE CASCADE"
            ",allowkey TEXT NOT NULL"
            ",passwd TEXT UNIQUE NOT NULL"
            ",PRIMARY KEY (userid,allowkey)"
        ")",

        /** @brief Account logins table */
        "CREATE TABLE IF NOT EXISTS LoggedIn ("
            "userid INTEGER NOT NULL REFERENCES Owner (userid) ON DELETE CASCADE"
            ",PRIMARY KEY (userid)"
        ")",
        "DELETE FROM LoggedIn",

        /** @brief Modules table */
        "CREATE TABLE IF NOT EXISTS Modules ("
            "moduleId INTEGER PRIMARY KEY ASC NOT NULL"
            ",name TEXT UNIQUE NOT NULL"
            ",description TEXT DEFAULT \"\""
        ")",
        /** @brief Module dependency
        *   TODO: disallow cycles */
        "CREATE TABLE IF NOT EXISTS ModDepends ("
            "moduleId INTEGER NOT NULL REFERENCES Modules(moduleId) ON DELETE CASCADE"
            ",requiresId INTEGER NOT NULL REFERENCES Modules(moduleId) ON DELETE RESTRICT"
            ",PRIMARY KEY (moduleId,requiresId)"
        ")",
        /** @brief Scripts table */
        "CREATE TABLE IF NOT EXISTS Scripts ("
            "scriptId INTEGER PRIMARY KEY ASC NOT NULL"
            ",moduleId INTEGER NOT NULL REFERENCES Modules(moduleId) ON DELETE CASCADE"
            ",name TEXT NOT NULL"
            ",hash TEXT NOT NULL"
            ",source TEXT NOT NULL"
            ",luaver TEXT NOT NULL"
            ",binary BLOB NOT NULL"
            ",UNIQUE(moduleId,name)"
        ")",

        /** @brief objects in Blockiverse
        *   axis range: +- [0,2^29)
        *   Bx,By,Bz Blockiverse   coordinate (step ~149.45 parsec, max 80.2378 gigaparsec)
        *   Gx,Gy,Gz Galactic Zone coordiante (step ~8.5899 Gm, max ~149.45 parsec)
        *   Cx,Cy,Cz Chunk Zone    coordinate (step 16 m, ~ max ~8.5899 Gm)
        *   Px,Py,Pz Finepos       coordinate (step ~30 nm, max 16 m)
        *
        *   Finepos being a sub-chunk unit is not likely to participate
        *   in object-in-range search queries so the 2^29 limit does not apply
        *   as the 2^29 limit is only to allow square domain searches for any
        *   objects in range of some point (without a sqrt it is necessary to
        *   ensure the distance expansion in square domain does not overflow
        *   signed int64 given max magnitude of 2^63 (an axis magnitude doubles
        *   when sign bit added so when k=29 2^k magnitude is actually 2^30 thus
        *   the k+1 when you square it in the range calc 2^30 becomes 2^60, finally
        *   log(3*(2^60))/log(2) is 61.585 so would fit in 62 bits so all good.
        *   3*((2^(k+1))^2) <= 2^63  k=29 is largest k satisfying this limit
        */
        "CREATE TABLE IF NOT EXISTS PivotType ("
            "pivotTypeId INTEGER PRIMARY KEY ASC NOT NULL"
            ",ownerMod INTEGER NOT NULL REFERENCES Modules(moduleId) ON DELETE CASCADE"
            ",pivotType TEXT"
            ",UNIQUE(pivotType)"
        ")",
        "CREATE TABLE IF NOT EXISTS EntityType ("
            "entityTypeId INTEGER PRIMARY KEY ASC NOT NULL"
            ",ownerMod INTEGER NOT NULL REFERENCES Modules(moduleId) ON DELETE CASCADE"
            ",entityType TEXT"
            ",UNIQUE(entityType)"
        ")",
        "CREATE TABLE IF NOT EXISTS Entity ("
            "entityId INTEGER PRIMARY KEY ASC NOT NULL"
            ",entityType INTEGER NOT NULL REFERENCES EntityType(entityTypeId) ON DELETE CASCADE"
            // identity that must be unique for a particular entityType
            ",objectId INTEGER NOT NULL"
            // when not null specifies a gravitational
            // reference object for coupling (eg: vehicle),
            // orbiting (eg: planet) or falling (eg: player)
            ",pivotId INTEGER REFERENCES Entity(entityId) ON DELETE SET NULL"
            ",pivotType INTEGER REFERENCES PivotType(pivotTypeId) ON DELETE SET NULL"
            // rotation
            ",rotX FLOAT NOT NULL DEFAULT 0.0"
            ",rotY FLOAT NOT NULL DEFAULT 0.0"
            ",rotZ FLOAT NOT NULL DEFAULT 0.0"
            // position in Blockiverse
            // (BxGxCxPx,ByGyCyPy,BzGzCzPz)
            ",Bx INTEGER NOT NULL DEFAULT 0"
            ",By INTEGER NOT NULL DEFAULT 0"
            ",Bz INTEGER NOT NULL DEFAULT 0"
            ",Gx INTEGER NOT NULL DEFAULT 0"
            ",Gy INTEGER NOT NULL DEFAULT 0"
            ",Gz INTEGER NOT NULL DEFAULT 0"
            ",Cx INTEGER NOT NULL DEFAULT 0"
            ",Cy INTEGER NOT NULL DEFAULT 0"
            ",Cz INTEGER NOT NULL DEFAULT 0"
            ",Px INTEGER NOT NULL DEFAULT 0"
            ",Py INTEGER NOT NULL DEFAULT 0"
            ",Pz INTEGER NOT NULL DEFAULT 0"
            ",UNIQUE(entityType,objectId)"
        ")",
        "CREATE TABLE IF NOT EXISTS Chunk ("
            "sha TEXT PRIMARY KEY NOT NULL"
            ",refcount INTEGER NOT NULL DEFAULT 1"
            ",data BLOB NOT NULL"
        ")",
        "CREATE TABLE IF NOT EXISTS ChunkRef ("
            "chunkRefId INTEGER PRIMARY KEY ASC NOT NULL"
            ",entityId INTEGER NOT NULL REFERENCES Entity(entityId)"
            ",sha TEXT NOT NULL REFERENCES Chunk(sha) ON DELETE RESTRICT "
            ",Cx INTEGER NOT NULL"
            ",Cy INTEGER NOT NULL"
            ",Cz INTEGER NOT NULL"
        ")",

        /** @brief Property definitions */
        "CREATE TABLE IF NOT EXISTS Property ("
            // property id
            "propId INTEGER PRIMARY KEY ASC NOT NULL"
            // module defining property
            ",ownerMod INTEGER NOT NULL REFERENCES Modules(moduleId) ON DELETE CASCADE"
            // type of property:
            // SQL_INTEGER,SQL_FLOAT,SQL_TEXT,SQL_BLOB
            ",type INTEGER NOT NULL"
            // property name
            ",name TEXT NOT NULL"
        ")",
        /** @brief Property values */
        "CREATE TABLE IF NOT EXISTS EntityData ("
            // entity the property data is assigned to
            "entityId INTEGER NOT NULL REFERENCES Entity(entityId) ON DELETE CASCADE"
            // which property
            ",propId INTEGER NOT NULL REFERENCES Property(propId) ON DELETE CASCADE"
            // property data
            ",value TEXT"
        ")"
    };

    /** @brief Search for account owned by specified client (pubkey) */
    const char *findOwner=
        "SELECT "
            "* "
        "FROM "
            "Owner "
        "WHERE "
            "userkey=?1";

    /** @brief Search for account using specified username */
    const char *findUser=
        "SELECT "
            "* "
        "FROM "
            "Owner "
        "WHERE "
            "username=?1";

    /** @brief Search for whitelist entry allowing access via specified client (pubkey) */
    const char *findAllowed=
        "SELECT "
            "* "
        "FROM "
            "AllowedClient "
        "WHERE "
            "userid=?1 "
            "AND allowkey=?2 "
            "AND passwd=?3";

    /** @brief Create new account for specified username and client (pubkey) */
    const char *createAccount=
        "INSERT "
            "INTO Owner "
                "(username,userkey) "
            "VALUES "
                "(?1,?2)";

    /** @brief Logs user into account
    *   Catchable constraint error on attempt to login
    *   to the same account more than once. */
    const char *loginAccount=
        "INSERT "
            "INTO LoggedIn "
                "(userid) "
            "VALUES "
                "(?1)";

    /** @brief Log user out of account */
    const char *logoutAccount=
        "DELETE "
            "FROM "
                "LoggedIn "
            "WHERE "
                "userid=?1";

};
