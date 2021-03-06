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

    namespace table {
        namespace Owner {
            enum _schema {
                userid=0,
                username=1,
                userkey=2
            };
        };
        namespace AllowedClient {
            enum _schema {
                userid=0,
                allowkey=1,
                passwd=2
            };
        };
        namespace LoggedIn {
            enum _schema {
                userid=0,
            };
        };
        namespace Modules {
            enum _schema {
                moduleId=0,
                name=1,
                description=2
            };
        };
        namespace ModDepends {
            enum _schema {
                modeId=0,
                requiresId=1
            };
        };
        namespace Scripts {
            enum _schema {
                scriptId=0,
                moduleId=1,
                name=2,
                hash=3,
                source=4,
                luaver=5,
                binary=6
            };
        };
        namespace PivotType {
            enum _schema {
                pivotTypeId=0,
                onwerMod=1,
                pivotType=2
            };
        };
        namespace EntityType {
            enum _schema {
                entityTypeId=0,
                onwerMod=1,
                entityType=2
            };
        };
        namespace Entity {
            enum _schema {
                entityId=0,
                entityType=1,
                pivotId=2,
                pivotType=3,
                Bx=4,
                By=5,
                Bz=6,
                Gx=7,
                Gy=8,
                Gz=9,
                Cx=10,
                Cy=11,
                Cz=12,
                Px=13,
                Py=14,
                Pz=15
            };
        };
        namespace Property {
            enum _schema {
                propId=0,
                Owner=1,
                Type=2,
                Name=3
            };
        };
        namespace EntityData {
            enum _schema {
                entityId=0,
                propId=1,
                Value=2
            };
        };
    };

    namespace result {
        namespace findOwner {
            using namespace bvquery::table::Owner;
        };
        namespace findUser {
            using namespace bvquery::table::Owner;
        };
        namespace findAllowed {
            using namespace bvquery::table::AllowedClient;
        };
    };

    /** @brief Iterable for initializing all tables via loop */
    extern std::vector<const char *> init_tables;

    /** @brief Search for account owned by specified client (pubkey) */
    extern const char *findOwner;
    /** @brief Search for account using specified username */
    extern const char *findUser;
    /** @brief Search for whitelist entry allowing access via specified client (pubkey) */
    extern const char *findAllowed;
    /** @brief Create new account for specified username and client (pubkey) */
    extern const char *createAccount;
    /** @brief Logs user into account
    *   Catchable constraint error on attempt to login
    *   to the same account more than once. */
    extern const char *loginAccount;
    /** @brief Log user out of account */
    extern const char *logoutAccount;

};

#endif // BV_QUERIES_HPP_INCLUDED
