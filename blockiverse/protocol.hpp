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
#include <boost/any.hpp>
#include <map>
#include <stack>
#include <queue>
#include <windows.h>

namespace bvnet {

    extern u32 reg_objects_softmax;

    struct obref {
        /* wrapper for object ref */
        u32 id;
        obref(u32 i):id(i) {}
        ~obref() {}
    };
    struct ob_is_gone {
        /* wrapper for object-gone message */
        u32 id;
        ob_is_gone(u32 i):id(i) {}
        ~ob_is_gone() {}
    };

    class object;
    class registry;
    class connection;
    class protocol;

    typedef std::map<u32,object*> object_map;
    typedef std::stack<boost::any> value_stack;
    typedef std::queue<boost::any> value_queue;
    typedef object_map::iterator omap_iter;

    class registry_full : public std::exception {
        mutable char buf[80];
        virtual const char *what() const throw();
    };
    class object_null : public std::exception {
        virtual const char *what() const throw();
    };
    class session_abandoned : public std::exception {
        virtual const char *what() const throw();
    };
    class windows_error : public std::exception {
        mutable char buf[80];
        virtual const char *what() const throw();
        public:
        windows_error(const char *fcn, DWORD err)
            {snprintf(buf,sizeof(buf),"%s error: %ld",fcn,err);}
    };

    class unlock_on_destroy {
        HANDLE lock;
    public:
        unlock_on_destroy(HANDLE theLock) : lock(theLock) {}
        ~unlock_on_destroy() {
            ReleaseMutex(lock);
        }
    };

    class session {
    protected:
        /* value stack */
        value_stack argstack;
        value_queue sendq;
        /* registered objects in session */
        registry *reg;
        /* mutex on session manipulation */
        HANDLE syncro;
    public:
        session();
        virtual ~session();
        /* tells other end specified object no longer valid */
        void notify_remove(u32 id);
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
        session *listener;
        /* registration of objects */
        object_map objects;
    public:
        /* construction/destruction */
        registry() : next_slot(1), listener(NULL) {}
        virtual ~registry();
        /* add object to registry */
        int register_object(object *ob);
        /* set upstream removal notify sink */
        void set_listener(session *sink) {listener=sink;}
        void notify(u32 id);
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
        if (objects.size()>=reg_objects_softmax) {
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
        }

        /*
            For security do not assume next_slot+1
            is an unoccupied slot.

            An attacker could use rapid creation and deletion of
            objects to execute a wraparound attack on next_slot
            then register a new object in a low numbered slot to
            overwrite an existing object in the registry.
        */
        omap_iter scan=objects.find(next_slot);
        while (scan!=objects.end()) {
            ++next_slot;
            /* skip 0 as it's reserved */
            if (next_slot==0) ++next_slot;
            scan=objects.find(++next_slot);
        }

        /* invariant: objects[next_slot] empty
        **   register object
        */
        objects[next_slot]=ob;
        return next_slot;
    }

    inline void registry::notify(u32 id) {
        if (listener!=NULL) {
            listener->notify_remove(id);
        }
    }

    inline bool registry::unregister(u32 id) {
        /*
        **  remove object from registry
        **  and notify upstream event sink
        */
        omap_iter victim;
        victim=objects.find(id);
        if (victim!=objects.end()) {
            notify(id);
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
        omap_iter cur;

        if (ob==NULL) {
            /* indicate not found if NULL */
            return false;
        }

        cur=objects.begin();
        while (cur!=objects.end()) {
            if (cur->second==ob) {
                notify(cur->first);
                objects.erase(cur);
                return true;
            }
            else {
                ++cur;
            }
        }
        return false;
    }

    inline registry::~registry() {
        /*
        ** destructor must cleanly clear/notify
        ** any remaining objects
        */
        omap_iter remain=objects.begin();
        while (remain!=objects.end()) {
            notify(remain->first);
            /* post-increment iter to keep valid */
            objects.erase(remain++);
        }
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

    inline const char *session_abandoned::what() const throw() {
        return "Error: Prior thread controlling session aborted uncleanly.";
    }

    /*
    **  connection inlines
    */
    inline session::session() {
        syncro=CreateMutex(NULL,FALSE,NULL);
        if (syncro==NULL) {
            throw windows_error("CreateMutex",GetLastError());
        }
    }
    inline session::~session() {
        CloseHandle(syncro);
    }
    inline void session::notify_remove(u32 id) {
        DWORD wState=WaitForSingleObject(syncro,INFINITE);
        switch (wState) {
        case WAIT_OBJECT_0:
            try {
                unlock_on_destroy lock(syncro);
                sendq.push(ob_is_gone(id));
            }
            catch (...) {throw;}
            break;
        case WAIT_ABANDONED:
            throw session_abandoned();
        }
    }

};  // bvnet

#endif // BV_PROTOCOL_HPP_INCLUDED
