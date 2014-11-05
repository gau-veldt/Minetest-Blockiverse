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

    extern u32 reg_objects_softmax;
    class object;
    typedef std::map<u32,object*> object_map;

    class registry_full : public std::exception {
        mutable char buf[80];
        virtual const char *what() const throw();
    };

    class object_null : public std::exception {
        virtual const char *what() const throw();
    };

    class event_sink {
    private:
    protected:
    public:
        event_sink() {}
        void notify_remove(int id);
        virtual ~event_sink() {}
    };

    class protocol {
    private:
    protected:
    public:
        protocol() {}
        virtual ~protocol() {}
    };

    /*
    ** registry tracks active
    ** object references
    **
    ** instances are permitted
    **
    ** server-side: one instance for each connected client
    ** client-side: one sole instance for upstream server
    **
    ** NB: registry does not take ownership of registered
    **     objects instead, object is designed to require
    **     a registry reference at construction
    */
    class registry {
    private:
        /* next id to use for object registration */
        u32 next_slot;
        /* notify sink for object removals */
        event_sink *listener;
        /* registration of objects */
        object_map objects;
    public:
        /* construction/destruction */
        registry() : next_slot(1), listener(NULL) {}
        virtual ~registry() {}
        /* add object to registry */
        int register_object(object *ob);
        /* set upstream removal notify sink */
        void set_listener(event_sink *sink) {listener=sink;}
        /* remove object from registry */
        bool unregister(u32 id);
        bool unregister(object *ob);
    };

    /*
    ** object is base used for objects
    ** exchangeable via object references
    **
    ** since secure referencing requires a way to
    ** track object lifetime a registry reference
    ** is required for construction
    */
    class object {
    protected:
        registry &store;
    public:
        object(registry &reg) :
            store(reg) {
                store.register_object(this);
            }
        virtual ~object() {
            store.unregister(this);
        }
    };

    /*
    **  Registry inlines
    */
    inline int registry::register_object(object *ob) {
        /*
        **  Insert object ob into registry and return slot id
        **  the object was registered to.
        */
        if (ob==NULL)
            /* refuse to register a NULL pointer */
            throw object_null();
        if (objects.size()>=reg_objects_softmax)
            /*
            ** enforce an object store softmax
            **
            ** A softmax exception can be caught and the offending session
            ** simply disconnected whether due to a configuration/program
            ** error (softmax too low), or a deliberate attack by a malicious
            ** client attempting to create enough objects to exhaust the free
            ** store hardmax which may lead to a system crash and subsequent
            ** collateral damage.
            */
            throw registry_full();
        /*
            For security do not assume next_slot+1
            is an unoccupied slot.

            An attacker could use rapid creation and deletion of
            objects to execute a wraparound attack on next_slot
            then register a new object in a low numbered slot to
            overwrite an existing object in the registry.
        */
        object_map::iterator scan;
        scan=objects.find(next_slot);
        while (scan!=objects.end()) {
            ++next_slot;
            /* skip 0 as it's reserved */
            if (next_slot==0) ++next_slot;
            scan=objects.find(++next_slot);
        }

        /* next_slot is safe so register object */
        objects[next_slot]=ob;
        return next_slot;
    }

    inline bool registry::unregister(u32 id) {
        /*
        **  remove object from registry
        **  and notify upstream event sink
        */
        object_map::iterator victim;
        victim=objects.find(id);
        if (victim!=objects.end()) {
            objects.erase(victim);
            return true;
        }
        return false;
    }

    inline bool registry::unregister(object *ob) {
        /*
        **  remove object from registry
        **  and notify upstream event sink
        */
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

    /*
    **  Error message inlines
    */
    inline const char *registry_full::what() const throw() {
        snprintf(buf,sizeof(buf),
                 "Registry full: exceeded %d maximum objects.",
                 reg_objects_softmax);
        return buf;
    }

    inline const char *object_null::what() const throw() {
        return "Error: Attempt to register NULL as object.";
    }

};  // bvnet

#endif // BV_PROTOCOL_HPP_INCLUDED
