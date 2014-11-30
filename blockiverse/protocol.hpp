/*
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
*/
/** @mainpage Minetest-Blockiverse
**
**  @version 0.0.1
**  @date 2014
**  @copyright GNU LGPL 3.0
**
**  @section bv_banner Blockiverse
**
**  An extension based on minetest allowing for space travel
**  between distant worlds, blockships and other vehicles,
**  extraplanetary resource collection, celestial bodies,
**  space exploration and space platforms allowing orbital
**  habitation.
**
**  Incorporates portions of code from minetest 0.4.10-dev
**
**  Copyright (C) 2014 Brian Jack <gau_veldt@hotmail.com>
**  Distributed as free software using the copyleft
**  LGPL Version 3 license:
**  https://www.gnu.org/licenses/lgpl-3.0.en.html
**
*/
#ifndef BV_PROTOCOL_H_INCLUDED
#define BV_PROTOCOL_H_INCLUDED

#include "common.hpp"
#include <iomanip>
#include <typeinfo>
#include <exception>
#include <stack>
#include <queue>
#include <boost/any.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/bimap.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>

using boost::asio::ip::tcp;
typedef boost::asio::io_service io_service;

extern int log2_tbl[];
extern int pow2_tbl[];

typedef boost::function<void()> lpvFunc;

namespace bvnet {
    /** Maximum size of object registry */
    extern u32 reg_objects_softmax;

    /**
    * @brief Possible value types.
    *
    * These are the possible types of values transmitted
    * between server/client sessions.
    */
    typedef enum {
        vtInt=1,        /**< Variable-length integer */
        vtFloat=2,      /**< Floating-point value */
        vtBlob=3,       /**< Arbitrary binary data (currently same as String) */
        vtString=4,     /**< Length-prefixed string */
        vtObref=5,      /**< Object reference */
        vtDeath=6,      /**< Object no longer exists */
        vtMethod=7      /**< Object method call */
    } valtype;
    /** map of protocol valuetype class to corresponding data type */
    typedef std::map<const char*,valtype> type_map;
    /** used by serialization to encode or decode protocol valuetypes to actual data types */
    extern type_map typeMap;

    /** protocol valuetype class for object reference */
    struct obref {
        /* wrapper for object ref */
        u32 id;
        obref(u32 i):id(i) {}
        ~obref() {}
    };
    /** protocol valuetype class for indicating object no longer exists */
    struct ob_is_gone {
        /* wrapper for object-gone message */
        u32 id;
        ob_is_gone(u32 i):id(i) {}
        ~ob_is_gone() {}
    };
    /** protocol valuetype class for object method calls */
    struct method_call {;
        /* wrapper for method call message */
        u32 id;
        u32 idx;
        lpvFunc callbk;
        size_t rcount;
        method_call(u32 i,u32 m,lpvFunc cb,int rnum):id(i),idx(m),callbk(cb),rcount(rnum) {}
        ~method_call() {}
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

    /** incoming value stack to receive incoming data */
    typedef std::stack<boost::any> value_stack;
    /** queued outgoing data waiting for transfer */
    typedef std::queue<boost::any> value_queue;
    typedef std::map<u32,bool> proxy_map;
    typedef std::queue<method_call> cb_queue;
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
    class argstack_empty : public std::exception {
        virtual const char *what() const throw();
    };

    /**
    *   @brief Symmetrical endpoint session for established connection.
    *
    *   Maintains the state of the session while the endpoint is connected.
    *   The protocol is mainly symmetrical -- both endpoints have the same
    *   communication language.  Once a bootstrap/hnadshake announces the
    *   root object the session will asynchronously wait for incoming data
    *   with the possibility of sending data or method calls upstream to
    *   the remote (and symmetrically processing any such data or method
    *   calls received from the remote).
    */
    class session {
    private:
        io_service *io_;            /**< io_service for this session  */
        object *root;               /**< session's root (bootstrap) object */
        bool isActive;              /**< connection is alive */
        bool isBooting;             /**< session is in handshake/bootstrap phase */
        bool _float_or_semi;        /**< expecting floating point chars or terminating ; */
        bool _neg_int;              /**< got - meaning incoming int is a negative */
        bool _opcode_read_queued;   /**< when true already waiting for opcode to arrive */
        std::string _fpstr;         /**< accumulator to receive incoming floating point value  */
        char in_ch;                 /**< the character just received */
        char in_idx[4];             /**< the uint32 just received */
        char in_s64[8];             /**< the sint64 just received */
    protected:
        void on_recv(const boost::system::error_code &ec,size_t rlen);
        void on_recv_dead_obid(const boost::system::error_code &ec);
        void on_recv_call_obid(const boost::system::error_code &ec);
        void on_recv_call_idx(const boost::system::error_code &ec,u32 obid);
        void on_recv_oref(const boost::system::error_code &ec,size_t rlen);
        void on_recv_str(const boost::system::error_code &ec,char *buf);
        void on_recv_len(const boost::system::error_code &ec,size_t rlen);
        void on_recv_s64(const boost::system::error_code &ec,u32 bsize);
        void on_write_done(std::string *finsihedbuf);
        void on_write_call_done(std::string *finishedbuf,lpvFunc cb);
        /** notifies callback when expected number of return arguments arrive */
        void check_argnotify();

        /* value stack */
        value_stack argstack;   /**< incoming results stack */
        value_queue sendq;      /**< outgoing values queue */
        proxy_map   proxy;      /**< cache of available remote objects */
        cb_queue    argnotify;  /**< callbacks to notify when remote methods complete */

        registry *reg;      /**< registered objects in session */
        mutex *synchro;     /**< mutex on session manipulation */
        tcp::socket *conn;  /**< connection */

        u32 remoteRoot;     /**< once booted has remote_root */
    public:
        session();
        virtual ~session();

        void notify_remove(u32 id);                 /**< signals that object no longer valid */
        void disconnect() {isActive=false;}         /**< close the conncetion */
        int register_object(object *o);             /**< register object as available to remote */
        u32 getRemote() {return remoteRoot;}        /**< get remote's root (bootstrap) object */
        bool hasRemote() {return 0!=remoteRoot;}    /**< true indicates remote root object valid */
        bool unregister(u32 id);                    /**< unregister object by-id and inform remote */
        bool unregister(object *ob);                /**< unregister object by-address and inform remote */
        void encode(const boost::any &a);           /**< sends any queued values over the wire */
        /**
        * Determine type of result stack top value.
        * @throw argstack_empty if the result stack is empty when attempted
        */
        valtype argtype() {
            if (argstack.size()>0) {
                return typeMap[argstack.top().type().name()];
            }
            throw argstack_empty();
        }
        /** current size of result stack */
        int argcount() {return argstack.size();}
        /**
        * @brief Receive next value from top of return stack.
        *
        * Since the value stack holds arbitrary types getarg is templated
        * in order to provide a different return value type for all value
        * types possible to be received.
        *
        * @throw argstack_empty if the result stack is empty when attempted
        */
        template<typename V>
        V getarg() {
            if (argstack.size()>0) {
                V rc=boost::any_cast<V>(argstack.top());
                argstack.pop();
                return rc;
            }
            throw argstack_empty();
        }
        /** Handshake/bootstrap process */
        void bootstrap(object *root);
        void send_int(s64 val) {sendq.push(val);}
        //void send_int(s64 &val) {sendq.push(val);}
        void send_float(float val) {sendq.push(val);}
        //void send_float(float &val) {sendq.push(val);}
        void send_blob(std::string val) {sendq.push(val);}
        //void send_blob(std::string &val) {sendq.push(val);}
        void send_string(std::string val) {sendq.push(val);}
        //void send_string(std::string &val) {sendq.push(val);}
        void send_obref(u32 id) {sendq.push(obref(id));}
        void send_call(u32 id,u32 m,lpvFunc cb=NULL,int rcount=0) {sendq.push(method_call(id,m,cb,rcount));}
        /** @brief Queries if an object still valid  and useable on remote.
        *   @param obid object id.
        *   @return true if oject useable false otherwise.
        */
        bool isValidObject(u32 obid) {
            return (proxy.find(obid)!=proxy.end());
        }
        /** network pump - blocks until data received. */
        bool run();
        /** network pump - returns to allow other activy whilst waiting. */
        bool poll();
        value_queue &getSendQueue() {return sendq;}
        mutex &getMutex() {return *synchro;}
        void set_conn(tcp::socket &s) {
            conn=&s;
            io_=&(s.get_io_service());
            LOCK_COUT
            std::cout << "session [" << this << "] on socket " << conn << " via io=" << io_ << std::endl;
            UNLOCK_COUT
        }
        /** for debugging - dumps data about session */
        void dump(std::ostream &os);
    };

    /**
    ** registry tracks active
    ** object references.
    **
    ** instances are permitted.
    **
    ** server-side: one instance for each connected client
    ** client-side: one sole instance for upstream server.
    **
    ** NB: Registry does not take ownership of registered
    **     objects instead, object is designed to require
    **     a registry reference at construction.
    */
    class registry {
    private:
        mutex *synchro;     /**< mutex on registry manipulation */
        session *listener;  /**< notify sink for object removals */

        u32 next_slot;          /**< id for next object [must be thread-synced] */
        object_map objects;     /**< registered objects [must be thread-synced] */
    public:
        /** construction/destruction */
        registry(session *host) : listener(host),next_slot(1)
            {synchro=new mutex();}
        virtual ~registry();
        /** add object to registry */
        int register_object(object *ob);
        /** notify upstream of object destruction */
        void notify(u32 id);
        /** remove object from registry by-id */
        bool unregister(u32 id);
        /** remove object from registry by-ptr */
        bool unregister(object *ob);
        /** @brief query id for given object ptr @param ob object ptr @return object id */
        u32 idOf(object* ob);
        /** @brief query ptr for given object id @param id object id @return object ptr */
        object* obOf(u32 id);
        /** for debugging - dumps data about registry */
        void dump(std::ostream &os);
    };

    /**
    ** object is base used for objects
    ** exchangeable via object references.
    **
    ** since secure referencing requires a way to
    ** track object lifetime a registry reference
    ** is required for construction.
    */
    class object {
    protected:
        session &ctx;       /**< for objects to attach to the session's registry */
    public:
        /** construction of an object requires reference to the session to attach to */
        object(session &sess) :
            ctx(sess) {
                LOCK_COUT
                std::cout << "object [" << this << "] ctor" << std::endl;
                UNLOCK_COUT
                ctx.register_object(this);
            }
        /** base dtor to automatically unregister the object */
        virtual ~object() {
            LOCK_COUT
            std::cout << "object [" << this << "] dtor" << std::endl;
            UNLOCK_COUT
            ctx.unregister(this);
        }
        /**
        *   @brief Get object's identity.
        *   @return Object identity string
        *
        *   Overriden by superclass to announce it's identity.
        */
        virtual const char *getType() {return "baseObject";}
        /**
        *   @brief Method call switchboard.
        *
        *   Overidden by superclass to implement methods callable
        *   by the remote.  Currently the superclasses are using
        *   big switchbanks which looks plain evil but at this
        *   point I'm not sure of what to refactor with.
        *
        *   @todo
        *   It would be good for the base class to implement some
        *   sort of glue to take out the switch boilerplate and some
        *   sort of static enum to get rid of those magic numbers
        *   (method call ids from remote POV) and
        *
        */
        virtual void methodCall(unsigned int idx)=0;
    };

    /*
    **  Registry inlines
    */
    inline int registry::register_object(object *ob) {
        int chosen_slot; // local value other threads can't modify
        /**
        **  Insert object ob into registry and return slot id
        **  the object was registered to.
        **  @param ob ptr to object to register
        **  @return slot id object registered to
        **  @throw object_null I refuse to register NULLs
        **  @throw registry_full Reigstry exceeded size limit - DDoS mitigation.
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
    inline const char *argstack_empty::what() const throw() {
        return "Object access when argument stack is empty.";
    }

    /*
    **  connection inlines
    */
    inline session::session() {
        synchro=new mutex();
        reg=new registry(this);
        conn=NULL;
        isBooting=true;
        _float_or_semi=false;
        _neg_int=false;
        _opcode_read_queued=false;
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
    inline void session::on_write_done(std::string *finishedbuf) {
        delete finishedbuf;
    }
    inline void session::on_write_call_done(std::string *finishedbuf,lpvFunc cb) {
        delete finishedbuf;
        cb();
    }
    inline void session::on_recv_oref(const boost::system::error_code &ec,size_t rlen) {
        if (isActive) {
            if (!ec) {
                u32 idx=*((u32*)in_idx);
                proxy[idx]=true;
                if (isBooting) {
                    LOCK_COUT
                    std::cout << "session [" << this
                              << "] booted: remote root=" << idx
                              << std::endl;
                    UNLOCK_COUT
                    remoteRoot=idx;
                    // booted once root known
                    isBooting=false;
                } else {
                    argstack.push(obref(idx));
                    check_argnotify();
                }
            } else {
                LOCK_COUT
                std::cout << "session [" << this
                          << "] expected objectref got EOF"
                          << " (" << ec << ")"
                          << std::endl;
                UNLOCK_COUT
                isActive=false;
            }
        }
    }
    inline void session::on_recv_str(const boost::system::error_code &ec,char* buf) {
        if (!ec) {
            argstack.push(std::string(buf));
            check_argnotify();
        } else {
            LOCK_COUT
            std::cout << "session [" << this
                      << "] expected string data got EOF"
                      << " (" << ec << ")"
                      << std::endl;
            UNLOCK_COUT
            isActive=false;
        }
        delete [] buf;
    }
    inline void session::on_recv_len(const boost::system::error_code &ec,size_t rlen) {
        if (isActive) {
            if (!ec) {
                u32 idx=*((u32*)in_idx);
                char* pstr=new char[1+idx];
                pstr[idx]='\0';
                boost::asio::async_read(
                    *conn,
                    boost::asio::buffer(pstr,idx),
                    boost::bind(&session::on_recv_str,this,
                        boost::asio::placeholders::error,
                        pstr));

            } else {
                LOCK_COUT
                std::cout << "session [" << this
                          << "] expected string len got EOF"
                          << " (" << ec << ")"
                          << std::endl;
                UNLOCK_COUT
                isActive=false;
            }
        }
    }
    inline void session::on_recv_dead_obid(const boost::system::error_code &ec) {
        if (isActive) {
            if (!ec) {
                u32 obid=*((u32*)in_idx);
                /* proxy eliminates need for death messages in argstack */
                proxy.erase(obid);
            } else {
                LOCK_COUT
                std::cout << "session [" << this
                          << "] expected dead objectid got EOF"
                          << " (" << ec << ")"
                          << std::endl;
                UNLOCK_COUT
                isActive=false;
            }
        }
    }
    inline void session::on_recv_call_obid(const boost::system::error_code &ec) {
        if (isActive) {
            if (!ec) {
                u32 obid=*((u32*)in_idx);
                boost::asio::async_read(
                    *conn,
                    boost::asio::buffer(in_idx,4),
                    boost::bind(&session::on_recv_call_idx,this,
                        boost::asio::placeholders::error,
                        obid));
            } else {
                LOCK_COUT
                std::cout << "session [" << this
                          << "] expected callee objectid got EOF"
                          << " (" << ec << ")"
                          << std::endl;
                UNLOCK_COUT
                isActive=false;
            }
        }
    }
    inline void session::on_recv_call_idx(const boost::system::error_code &ec,u32 obid) {
        if (isActive) {
            if (!ec) {
                u32 idx=*((u32*)in_idx);
                object *ob=reg->obOf(obid);
                LOCK_COUT
                std::cout << "Session [" << this << "] call "
                          << ob->getType() << '[' << ob << "]." << idx
                          << std::endl;
                UNLOCK_COUT
                ob->methodCall(idx);
            } else {
                LOCK_COUT
                std::cout << "session [" << this
                          << "] expected method idx got EOF"
                          << " (" << ec << ")"
                          << std::endl;
                UNLOCK_COUT
                isActive=false;
            }
        }
    }
    inline void session::on_recv_s64(const boost::system::error_code &ec,u32 bsize) {
        if (isActive) {
            if (!ec) {
                s64 *in_val=(s64*)in_s64;
                s64 val=*in_val;
                if (_neg_int) val=-val;
                argstack.push(val);
                check_argnotify();
                _neg_int=false;
            } else {
                LOCK_COUT
                std::cout << "session [" << this
                          << "] expected " << bsize << "-byte int got EOF"
                          << " (" << ec << ")"
                          << std::endl;
                UNLOCK_COUT
                isActive=false;
            }
        }
    }
    inline void session::on_recv(const boost::system::error_code &ec,size_t rlen) {
        _opcode_read_queued=false;
        if (isActive) {
            if (!ec) {
                int ich=int((unsigned char)in_ch);
                if (_float_or_semi) {
                    if (in_ch==';') {
                        _float_or_semi=false;
                        float flt;
                        std::istringstream cvt(_fpstr);
                        cvt >> flt;
                        argstack.push(flt);
                        check_argnotify();
                    } else {
                        _fpstr+=in_ch;
                    }
                } else {
                    u32 bsize;
                    switch(in_ch) {
                /* integer value of 2^N bytes */
                    case '0':
                    case '1':
                    case '2':
                    case '3':
                  /*case '4':
                    case '5':
                    case '6':
                    case '7':
                    case '8':
                    case '9':*/
                        bsize=pow2_tbl[in_ch-'0'];
                        *((s64*)in_s64)=0;
                        boost::asio::async_read(
                            *conn,
                            boost::asio::buffer(in_s64,bsize),
                            boost::bind(&session::on_recv_s64,this,
                                boost::asio::placeholders::error,
                                bsize));
                        break;
                    case '-':
                        _neg_int=true;
                        break;
                    case '+':
                        _neg_int=false;
                        break;
                    case 'f':
                        _float_or_semi=true;
                        _fpstr.clear();
                        break;
                    case '"':
                    case 'b':
                        boost::asio::async_read(
                            *conn,
                            boost::asio::buffer(in_idx,4),
                            boost::bind(&session::on_recv_len,this,
                                boost::asio::placeholders::error,
                                boost::asio::placeholders::bytes_transferred));
                        break;
                    case 'o':
                        boost::asio::async_read(
                            *conn,
                            boost::asio::buffer(in_idx,4),
                            boost::bind(&session::on_recv_oref,this,
                                boost::asio::placeholders::error,
                                boost::asio::placeholders::bytes_transferred));
                        break;
                    case '.':
                        boost::asio::async_read(
                            *conn,
                            boost::asio::buffer(in_idx,4),
                            boost::bind(&session::on_recv_call_obid,this,
                                boost::asio::placeholders::error));
                        break;
                    case '~':
                        boost::asio::async_read(
                            *conn,
                            boost::asio::buffer(in_idx,4),
                            boost::bind(&session::on_recv_dead_obid,this,
                                boost::asio::placeholders::error));
                        break;
                    default:
                        LOCK_COUT
                        std::cout << "session [" << this << "] recv len="
                                  << rlen << " 0x"
                                  << std::setw(2) << std::setfill('0') << std::hex << ich;
                        if (ich>31 && ich<128)
                            std::cout << ' ' << in_ch;
                        std::cout << std::endl;
                        UNLOCK_COUT
                    }
                }
            } else {
                LOCK_COUT
                if (_float_or_semi) {
                    std::cout << "session [" << this
                              << "] expected floatnum; got EOF"
                              << " (" << ec << ")"
                              << std::endl;

                } else {
                    std::cout << "session [" << this
                              << "] expected opcode got EOF"
                              << " (" << ec << ")"
                              << std::endl;
                }
                UNLOCK_COUT
                isActive=false;
            }
        }
    }
    inline void session::check_argnotify() {
        /**
        **  Notifies stored callback when argstack reaches specified size
        */
        if (argnotify.size()>0) {
            size_t args_avail=argstack.size();
            if (args_avail>=argnotify.front().rcount) {
                method_call mc(argnotify.front());
                argnotify.pop();
                mc.callbk();
            }
        }
    }
    inline bool session::run() {
        try {
            if (isActive) {
                if (io_->stopped())
                    io_->reset();
                while (sendq.size()>0) {
                    encode(sendq.front());
                    sendq.pop();
                }
                if (!_opcode_read_queued) {
                    _opcode_read_queued=true;
                    boost::asio::async_read(
                        *conn,
                        boost::asio::buffer(&in_ch,1),
                        boost::bind(&session::on_recv,this,
                            boost::asio::placeholders::error,
                            boost::asio::placeholders::bytes_transferred));
                }
                io_->run();
            }
        } catch (std::exception &e) {
            isActive=false;
            LOCK_COUT
            std::cout << "Session [" << this << "] closed: "
                      << e.what() << std::endl;
            UNLOCK_COUT
            isActive=false;
        }
        return isActive;
    }
    inline bool session::poll() {
        try {
            if (isActive) {
                if (io_->stopped())
                    io_->reset();
                while (sendq.size()>0) {
                    encode(sendq.front());
                    sendq.pop();
                }
                if (!_opcode_read_queued) {
                    _opcode_read_queued=true;
                    boost::asio::async_read(
                        *conn,
                        boost::asio::buffer(&in_ch,1),
                        boost::bind(&session::on_recv,this,
                            boost::asio::placeholders::error,
                            boost::asio::placeholders::bytes_transferred));
                }
                io_->poll();
            }
        } catch (std::exception &e) {
            isActive=false;
            LOCK_COUT
            std::cout << "Session " << this << " error: " << e.what()
                      << " (connection closed)" << std::endl;
            UNLOCK_COUT
            isActive=false;
        }
        return isActive;
    }
    inline void session::encode(const boost::any &raw) {
        std::ostringstream ss;
        u32 idx;
        method_call mc(0,0,NULL,0);
        const char *idx_byte=(const char*)&idx;
        type_map::iterator tmi=typeMap.find(raw.type().name());
        if (tmi==typeMap.end()) {
            LOCK_COUT
            std::cout << "<unknown \"" << raw.type().name() << "\">" << std::endl;
            UNLOCK_COUT
        } else {
            s64 val;
            u64 mag,mag2;
            int log2,bytes;
            float flt;
            switch (tmi->second) {
            case vtInt:
                val=boost::any_cast<s64>(raw);
                mag=(val<0)?0-val:val;
                if (val<0) {
                    ss << '-';
                }
                bytes=1;
                mag2=mag;
                while (mag2>255) {
                    ++bytes;
                    mag2>>=8;
                }
                log2=log2_tbl[bytes-1];
                ss << log2;
                bytes=pow2_tbl[log2];
                while (bytes>0) {
                    ss << (const char)(mag&0xff);
                    mag>>=8;
                    --bytes;
                }
                break;
            case vtFloat:
                flt=boost::any_cast<float>(raw);
                ss << 'f' << flt << ';';
                break;
            case vtBlob:
            case vtString:
                idx=boost::any_cast<std::string>(raw).size();
                ss << '"'
                   << idx_byte[0] << idx_byte[1] << idx_byte[2] << idx_byte[3];
                ss << boost::any_cast<std::string>(raw);
                break;
            case vtObref:
                idx=boost::any_cast<obref>(raw).id;
                ss << 'o'
                   << idx_byte[0] << idx_byte[1] << idx_byte[2] << idx_byte[3];
                break;
            case vtDeath:
                idx=boost::any_cast<ob_is_gone>(raw).id;
                ss << '~'
                   << idx_byte[0] << idx_byte[1] << idx_byte[2] << idx_byte[3];
                break;
            case vtMethod:
                mc=boost::any_cast<method_call>(raw);
                LOCK_COUT
                std::cout << "call #" << mc.id << '.' << mc.idx
                          << "() cb=" << mc.callbk
                          << ", when stack+=" << mc.rcount
                          << std::endl;
                UNLOCK_COUT
                ss << '.';
                idx=mc.id;
                ss << idx_byte[0] << idx_byte[1] << idx_byte[2] << idx_byte[3];
                idx=mc.idx;
                ss << idx_byte[0] << idx_byte[1] << idx_byte[2] << idx_byte[3];
                break;
            }
            std::string *dynstr=new std::string(ss.str());
            if (tmi->second==vtMethod
                && mc.callbk) {
                // only if callback not null
                if (mc.rcount==0) {
                    // a callback of rcount 0 notifies
                    // when the queued method call processed
                    mc.callbk();
                } else {
                    // otherwise the notify is set for when
                    // argstack reaches size current_size+rcount
                    argnotify.push(mc);
                }
            }
            boost::asio::async_write(
                *conn,
                boost::asio::buffer(*dynstr,dynstr->size()),
                    boost::bind(&session::on_write_done,this,dynstr));
        }
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
