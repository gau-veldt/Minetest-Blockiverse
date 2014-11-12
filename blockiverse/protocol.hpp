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
#include <iomanip>
#include <typeinfo>
#include <stack>
#include <queue>
#include <boost/any.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/bimap.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>

using boost::asio::ip::tcp;
typedef boost::asio::io_service io_service;

// quick and dirty synchronized ostream
extern boost::mutex cout_mutex;
class scoped_cout_lock {
    std::ios state;
public:
    scoped_cout_lock() : state(NULL) {
        cout_mutex.lock();
        state.copyfmt(std::cout);
    }
    virtual ~scoped_cout_lock() {
        std::cout.flush();
        std::cout.copyfmt(state);
        cout_mutex.unlock();
    }
};
#define LOCK_COUT {scoped_cout_lock __scl;
#define UNLOCK_COUT }

namespace bvnet {
    extern u32 reg_objects_softmax;

    typedef enum {
        vtInt=1,
        vtFloat=2,
        vtBlob=3,
        vtString=4,
        vtObref=5,
        vtDeath=6
    } valtype;
    typedef std::map<const char*,valtype> type_map;
    extern type_map typeMap;

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

    typedef struct {} object_id;
    typedef struct {} object_addr;
    typedef boost::bimaps::tagged<u32,object_id> t_obId;
    typedef boost::bimaps::tagged<object*,object_addr> t_obAddr;
    typedef boost::bimap<t_obId,t_obAddr> object_map;
    typedef object_map::value_type ob_relation;
    typedef object_map::map_by<object_id>::iterator omap_iter_byid;
    typedef object_map::map_by<object_addr>::iterator omap_iter_byptr;
    typedef object_map::map_by<object_id>::type omap_select_id;
    typedef object_map::map_by<object_addr>::type omap_select_addr;
    typedef std::stack<boost::any> value_stack;
    typedef std::queue<boost::any> value_queue;
    typedef boost::mutex mutex;
    typedef boost::mutex::scoped_lock scoped_lock;

    class registry_full : public std::exception {
        mutable char buf[80];
        virtual const char *what() const throw();
    };
    class object_null : public std::exception {
        virtual const char *what() const throw();
    };
    class object_not_reg : public std::exception {
        virtual const char *what() const throw();
    };

    class session {
    private:
        io_service *io_;
        object *root;
        bool isActive;
        bool isBooting;
        char in_ch;
        char in_idx[4];
    protected:
        void on_recv(const boost::system::error_code &ec,size_t rlen);
        void on_recv_oref(const boost::system::error_code &ec,size_t rlen);

        /* value stack */
        value_stack argstack;
        value_queue sendq;

        registry *reg;      /* registered objects in session */
        mutex *synchro;     /* mutex on session manipulation */
        tcp::socket *conn;  /* connection */

        u32 remoteRoot;     /* once booted has remote_root */
    public:
        session();
        virtual ~session();

        void notify_remove(u32 id);         /* signals that object no longer valid */
        int register_object(object *o);
        u32 getRemote() {return remoteRoot;}
        bool hasRemote() {return 0!=remoteRoot;}
        bool unregister(u32 id);
        bool unregister(object *ob);
        void encode(const boost::any &a);
        void decode(const std::string &s);
        void bootstrap(object *root);
        bool run();
        value_queue &getSendQueue() {return sendq;}
        mutex &getMutex() {return *synchro;}
        void set_conn(tcp::socket &s) {
            conn=&s;
            io_=&(s.get_io_service());
        }

        void dump(std::ostream &os);
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
        mutex *synchro;     /* mutex on registry manipulation */
        session *listener;  /* notify sink for object removals */

        /* registration of objects */
        /* must_sync */ u32 next_slot;
        /* must_sync */ object_map objects;
    public:
        /* construction/destruction */
        registry(session *host) : listener(host),next_slot(1)
            {synchro=new mutex();}
        virtual ~registry();
        /* add object to registry */
        int register_object(object *ob);
        /* remove object from registry */
        void notify(u32 id);
        bool unregister(u32 id);
        bool unregister(object *ob);
        u32 idOf(object* ob);
        object* obOf(u32 id);

        void dump(std::ostream &os);
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
        session &ctx;
    public:
        object(session &sess) :
            ctx(sess) {
                LOCK_COUT
                std::cout << "object [" << this << "] ctor" << std::endl;
                UNLOCK_COUT
                ctx.register_object(this);
            }
        virtual ~object() {
            LOCK_COUT
            std::cout << "object [" << this << "] dtor" << std::endl;
            UNLOCK_COUT
            ctx.unregister(this);
        }
        virtual const char *getType() {return "baseObject";}
        virtual void methodCall(unsigned int idx)=0;
    };

    /*
    **  Registry inlines
    */
    inline int registry::register_object(object *ob) {
        int chosen_slot; // local value other threads can't modify
        /*
        **  Insert object ob into registry and return slot id
        **  the object was registered to.
        */

        if (ob==NULL) {
            /* refuse to register a NULL pointer */
            throw object_null();
        }

        {
            scoped_lock lock(*synchro);

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
            omap_select_id &table=objects.by<object_id>();
            omap_iter_byid row=table.find(next_slot);
            while (row!=table.end()) {
                ++next_slot;
                /* skip 0 as it's reserved */
                if (next_slot==0) ++next_slot;
                row=table.find(next_slot);
            }
            /* invariant: objects[next_slot] empty */

            /* make thread-safe copy for retval */
            chosen_slot=next_slot;

            /* register object */
            objects.insert(ob_relation(chosen_slot,ob));
        }
        return chosen_slot;
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
        scoped_lock lock(*synchro);

        omap_select_id &table=objects.by<object_id>();
        omap_iter_byid victim=table.find(id);
        if (victim!=table.end()) {
            notify(id);
            table.erase(victim);
            return true;
        }
        return false;
    }

    inline bool registry::unregister(object *ob) {
        /*
        **  remove object from registry
        **  and notify upstream event sink
        */
        if (ob==NULL) return false;     /* indicate not found if NULL */
        {
            scoped_lock lock(*synchro);

            omap_select_addr &table=objects.by<object_addr>();
            omap_iter_byptr victim=table.find(ob);
            if (victim!=table.end()) {
                notify(victim->get<object_id>());
                table.erase(victim);
                return true;
            }
            return false;
        }
    }

    inline u32 registry::idOf(object *ob) {
        /*
        **  get identify (objectref) of pointed object
        */
        if (ob==NULL) throw object_not_reg();   /* indicate DNE if NULL */
        {
            scoped_lock lock(*synchro);

            omap_select_addr &table=objects.by<object_addr>();
            omap_iter_byptr row=table.find(ob);
            if (row!=table.end()) {
                return row->get<object_id>();
            }
            throw object_not_reg();
        }
    }

    inline object* registry::obOf(u32 id) {
        /*
        **  get identify (objectref) of pointed object
        */
        if (id==0) throw object_not_reg();  /* object id==0 is reserved */
        {
            scoped_lock lock(*synchro);

            omap_select_id &table=objects.by<object_id>();
            omap_iter_byid row=table.find(id);
            if (row!=table.end()) {
                return row->get<object_addr>();
            }
            throw object_not_reg();
        }
    }

    inline registry::~registry() {
        {
            scoped_lock lock(*synchro);

            /*
            ** destructor must cleanly clear/notify
            ** any remaining objects
            */
            omap_select_id &table=objects.by<object_id>();
            omap_iter_byid row=table.begin();
            while (row!=table.end()) {
                notify(row->get<object_id>());
                /* post-increment iter to keep valid */
                table.erase(row++);
            }
        }
        delete synchro;
        synchro=NULL;
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
    inline const char *object_not_reg::what() const throw() {
        return "Object not in registry.";
    }

    /*
    **  connection inlines
    */
    inline session::session() {
        synchro=new mutex();
        reg=new registry(this);
        conn=NULL;
        isBooting=true;
        remoteRoot=0;
    }
    inline session::~session() {
        delete reg;
        delete synchro;
    }
    inline void session::notify_remove(u32 id) {
        scoped_lock lock(*synchro);
        sendq.push(ob_is_gone(id));
    }
    inline int session::register_object(object *o) {
        return reg->register_object(o);
    }
    inline bool session::unregister(u32 id) {
        return reg->unregister(id);
    }
    inline bool session::unregister(object *ob) {
        return reg->unregister(ob);
    }
    inline void session::bootstrap(object *sessionRoot) {
        boost::any a;
        root=sessionRoot;
        sendq.push(obref(reg->idOf(root)));
        isActive=true;
    }
    inline void session::on_recv_oref(const boost::system::error_code &ec,size_t rlen) {
        if (isActive) {
            if (!ec) {
                u32 idx=*((u32*)in_idx);
                if (isBooting) {
                    LOCK_COUT
                    std::cout << "session [" << this
                              << "] remote root=" << idx
                              << std::endl;
                    UNLOCK_COUT
                    remoteRoot=idx;
                    // booted once root known
                    isBooting=false;
                } else {
                    LOCK_COUT
                    std::cout << "session [" << this
                              << "] objectref=" << idx
                              << std::endl;
                    UNLOCK_COUT
                    argstack.push(obref(idx));
                }
            } else {
                LOCK_COUT
                std::cout << "session [" << this
                          << "] EOF"
                          << std::endl;
                UNLOCK_COUT
                isActive=false;
            }
        }
    }
    inline void session::on_recv(const boost::system::error_code &ec,size_t rlen) {
        if (isActive) {
            if (!ec) {
                int ich=int(in_ch);
                switch(in_ch) {
                case 'o':
                    boost::asio::async_read(
                        *conn,
                        boost::asio::buffer(in_idx,4),
                        boost::bind(&session::on_recv_oref,this,
                        boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred));
                    break;
                default:
                    LOCK_COUT
                    std::cout << "session [" << this << "] recv len="
                              << rlen << " 0x"
                              << std::setw(2) << std::setfill('0') << std::hex << (int)ich;
                    if (ich>31 && ich<128)
                        std::cout << ' ' << in_ch;
                    std::cout << std::endl;
                    UNLOCK_COUT
                }
            } else {
                LOCK_COUT
                std::cout << "session [" << this
                          << "] EOF"
                          << std::endl;
                UNLOCK_COUT
                isActive=false;
            }
        }
    }
    inline bool session::run() {
        if (isActive) {
            while (sendq.size()>0) {
                encode(sendq.front());
                sendq.pop();
            }
            boost::asio::async_read(
                *conn,
                boost::asio::buffer(&in_ch,1),
                boost::bind(&session::on_recv,this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
            io_->run();
            io_->reset();
        }
        return isActive;
    }
    inline void session::encode(const boost::any &raw) {
        std::ostringstream ss;
        u32 idx;
        const char *idx_byte=(const char*)&idx;
        type_map::iterator tmi=typeMap.find(raw.type().name());
        if (tmi==typeMap.end()) {
            LOCK_COUT
            std::cout << "<unknown \"" << raw.type().name() << "\">" << std::endl;
            UNLOCK_COUT
        } else {
            switch (tmi->second) {
            case vtInt:
                break;
            case vtFloat:
                break;
            case vtBlob:
                break;
            case vtString:
                break;
            case vtObref:
                idx=boost::any_cast<obref>(raw).id;
                ss << 'o'
                   << idx_byte[0] << idx_byte[1] << idx_byte[2] << idx_byte[3];
                break;
            case vtDeath:
                ss << 'o'
                   << idx_byte[0] << idx_byte[1] << idx_byte[2] << idx_byte[3]
                   << '~';
                break;
            }
            boost::asio::write(
                *conn,
                boost::asio::buffer(ss.str(),ss.str().size()));
        }
    }
    inline void decode(const std::string &cooked) {
        boost::any a;
    }

    inline void session::dump(std::ostream &os) {
        LOCK_COUT
        os << "session object" << std::endl;
        os << "  argstack count: " << argstack.size() << std::endl;
        os << "  send queue size: " << sendq.size() << std::endl;
        os << "  socket: ";
        if (conn==NULL) {
            os << "<none>";
        } else {
            os << conn;
        }
        os << std::endl;
        UNLOCK_COUT
        reg->dump(os);
    }

    inline void registry::dump(std::ostream &os) {
        LOCK_COUT
        os << "  registry" << std::endl;
        os << "    objects registered: " << objects.size() << std::endl;
        object_map::iterator i=objects.begin();
        object *o;
        while (i!=objects.end()) {
            o=i->get<object_addr>();
            os << "      refid=" << i->get<object_id>() << " [";
            os << std::setfill('0') << std::setw(8) << std::hex << (u32)o;
            os << "] type=" << o->getType() << std::endl;
            ++i;
        }
        UNLOCK_COUT
    }

};  // bvnet

#endif // BV_PROTOCOL_HPP_INCLUDED
