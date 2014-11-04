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
**  Declaration (header) file protocol.hpp
**
**  Protocol for server/client comms header declarations
**
*/
#ifndef BV_PROTOCOL_H_INCLUDED
#define BV_PROTOCOL_H_INCLUDED

#include "common.hpp"
#include <map>

namespace bvnet {

    extern u32 max_reg_objects;
    class registry_full : public std::exception {
        mutable char buf[81];
        virtual const char *what() const throw() {
            snprintf(&buf[0],80,
                     "Registry full: %d maximum objects reached.",
                     max_reg_objects);
            return &buf[0];
        }
    };

    /*
    ** registry tracks active
    ** object references
    */
    class object;
    // registry
    // instances are permitted
    // server-side: one instance for each connected client
    // client-side: one sole instance for upstream server
    typedef std::map<u32,object*> object_map;
    class registry {
        private:
            // next id to use for object registration
            u32 next_slot;
            // registration of objects
            object_map objects;
        public:
            registry() : next_slot(1) {}
            virtual ~registry() {}

            int register_object(object *ob) {
                /*
                    Insert object ob into registry and return slot id
                    the object was registered to.  If the registry is
                    full (at maximum allowed entries) throws
                    registry_full

                    For security do not assume next_slot+1
                    is an unoccupied slot.

                    An attacker could use rapid creation and deletion of
                    objects to execute a wraparound attack on next_slot
                    then register a new object in a low numbered slot to
                    overwrite an existing object in the registry.
                */
                if (objects.size()>=max_reg_objects)
                    throw registry_full();
                object_map::iterator scan;
                scan=objects.find(next_slot);
                while (scan!=objects.end()) {
                    ++next_slot;
                    /* skip 0 as it's reserved */
                    if (next_slot==0) ++next_slot;
                    scan=objects.find(++next_slot);
                }
                /* safe to register object */
                objects[next_slot]=ob;
                return next_slot;
            }

            bool unregister_id(u32 id) {
                object_map::iterator victim;
                victim=objects.find(id);
                if (victim!=objects.end()) {
                    objects.erase(victim);
                    return true;
                }
                return false;
            }

            bool unregister_ob(object *ob) {
                object_map::iterator cur;
                if (ob==NULL) {
                    return false;
                }
                cur=objects.begin();
                while (cur!=objects.end()) {
                    if (cur->second==ob) {
                        objects.erase(cur);
                        return true;
                    }
                    else {
                        ++cur;
                    }
                }
                return false;
            }
    };

    /*
    ** object is base used for objects
    ** exchangeable via object references
    */
    class object {
    };

};

#endif // BV_PROTOCOL_HPP_INCLUDED
