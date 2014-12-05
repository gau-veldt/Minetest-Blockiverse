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
#include <memory>
#include <functional>
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
#include <boost/core/enable_if.hpp>
#include <boost/type_traits.hpp>

using boost::asio::ip::tcp;
typedef boost::asio::io_service io_service;

extern int log2_tbl[];
extern int pow2_tbl[];

typedef boost::function<void()> lpvFunc;

namespace bvnet {
    extern u32 reg_objects_softmax;

    using boost::enable_if;
    using boost::is_base_of;


    /**
    * @typedef valtype
    * @brief Possible value types.
    *
    * These are the possible types of values transmitted
    * between server/client sessions.
    */
    typedef enum {
        vtInt=1,        /**< @brief Variable-length integer */
        vtFloat=2,      /**< @brief Floating-point value */
        vtBlob=3,       /**< @brief Arbitrary binary data (currently same as String) */
        vtString=4,     /**< @brief Length-prefixed string */
        vtObref=5,      /**< @brief Object reference */
        vtDeath=6,      /**< @brief Object no longer exists */
        vtMethod=7,     /**< @brief Object method call */
        vtDMC=8         /**< @brief dmc message */
    } valtype;
    /** @typedef type_map @brief map of protocol valuetype class to corresponding data type */
    typedef std::map<const char*,valtype> type_map;
    /** @brief used by serialization to encode or decode protocol valuetypes to actual data types */
    extern type_map typeMap;

    /** @brief protocol valuetype class for object reference */
    struct obref {
        /* wrapper for object ref */
        u32 id;
        obref(u32 i):id(i) {}
        ~obref() {}
    };
    /** @brief protocol valuetype class for indicating object no longer exists */
    struct ob_is_gone {
        /* wrapper for object-gone message */
        u32 id;
        ob_is_gone(u32 i):id(i) {}
        ~ob_is_gone() {}
    };
    /** @brief protocol valuetype class for object method calls */
    struct method_call {;
        /* wrapper for method call message */
        u32 id;
        u32 idx;
        lpvFunc callbk;
        size_t rcount;
        method_call(u32 i,u32 m,lpvFunc cb,int rnum):id(i),idx(m),callbk(cb),rcount(rnum) {}
        ~method_call() {}
    };
    /** @brief protocol valuetype class for dmc messages */
    struct dmc_msg {
        u32 ob;
        string label;
        u32 slot;
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

    /** @brief incoming value stack to receive incoming data */
    typedef std::stack<boost::any> value_stack;
    /** @brief queued outgoing data waiting for transfer */
    typedef std::queue<boost::any> value_queue;
    typedef std::map<u32,bool> proxy_map;
    typedef std::queue<method_call> cb_queue;
    typedef boost::mutex mutex;
    typedef boost::mutex::scoped_lock scoped_lock;

    typedef std::function<void(object*,value_queue&)> dmcMethod;
    typedef std::map<unsigned int,dmcMethod> call_map;
    typedef std::map<unsigned int,string> name_map;
    typedef std::map<string,u32> indx_map;
    typedef std::map<u32,indx_map> iface_map;

    /** @brief Indicates registry exceeded reg_objects_softmax */
    class registry_full : public exception {
        mutable char buf[80];
        virtual const char *what() const throw();
    };
    /** @brief Indicates attempt to register NULL object */
    class object_null : public exception {
        virtual const char *what() const throw();
    };
    /** @brief Indicates referencing object not in registry */
    class object_not_reg : public exception {
        virtual const char *what() const throw();
    };
    /** @brief Indicates attempt to access value (or its type) when argstack empty */
    class argstack_empty : public exception {
        virtual const char *what() const throw();
    };
    class method_notimpl : public exception {
        mutable char buf[80];
        string dmcOb;
        unsigned int dmcMethodId;
        virtual const char *what() const throw();
    public:
        method_notimpl(string ob,unsigned int slot) :
            dmcOb(ob),dmcMethodId(slot) {}
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
    public:
        /** @brief when you want make dynamic objects on the session
        *
        *   Then have those objects automatically go away when the
        *   session ends if they don't die sooner
        */
        typedef std::shared_ptr<object> shared;
    private:
        /** @brief for session's dynamic object management */
        typedef std::map<u32,shared> gc_map;

        io_service *io_;            /**< @brief io_service for this session  */
        object *root;               /**< @brief session's root (bootstrap) object */
        bool isActive;              /**< @brief connection is alive */
        bool isBooting;             /**< @brief session is in handshake/bootstrap phase */
        bool _float_or_semi;        /**< @brief expecting floating point chars or terminating ; */
        bool _neg_int;              /**< @brief got - meaning incoming int is a negative */
        bool _opcode_read_queued;   /**< @brief when true already waiting for opcode to arrive */
        string _fpstr;         /**< @brief accumulator to receive incoming floating point value  */
        char in_ch;                 /**< @brief the character just received */
        char in_idx[4];             /**< @brief the uint32 just received */
        char in_s64[8];             /**< @brief the sint64 just received */
    protected:
        /** @brief various async data reception callbacks */
        void on_recv(const boost::system::error_code &ec,size_t rlen);
        /** @brief various async data reception callbacks */
        void on_recv_dead_obid(const boost::system::error_code &ec);
        /** @brief various async data reception callbacks */
        void on_recv_call_obid(const boost::system::error_code &ec);
        /** @brief various async data reception callbacks */
        void on_recv_call_idx(const boost::system::error_code &ec,u32 obid);
        /** @brief various async data reception callbacks */
        void on_recv_oref(const boost::system::error_code &ec,size_t rlen);
        /** @brief various async data reception callbacks */
        void on_recv_str(const boost::system::error_code &ec,char *buf);
        /** @brief various async data reception callbacks */
        void on_recv_len(const boost::system::error_code &ec,size_t rlen);
        /** @brief various async data reception callbacks */
        void on_recv_s64(const boost::system::error_code &ec,u32 bsize);
        /** @brief various async data reception callbacks */
        void on_recv_dmc_obid(const boost::system::error_code &ec);
        void on_recv_dmc_len(const boost::system::error_code &ec,dmc_msg mk_dmc);
        void on_recv_dmc_label(const boost::system::error_code &ec,dmc_msg mk_dmc,char* buf);
        void on_recv_dmc_slot(const boost::system::error_code &ec,dmc_msg mk_dmc);
        /** @brief various async trasnfer completion callbacks */
        void on_write_done(string *finsihedbuf);
        /** @brief various async trasnfer completion callbacks */
        void on_write_call_done(string *finishedbuf,lpvFunc cb);
        /** @brief notifies callback when expected number of return arguments arrive */
        void check_argnotify();

        value_stack     argstack;   /**< @brief incoming results stack */
        value_queue     sendq;      /**< @brief outgoing values queue */
        proxy_map       proxy;      /**< @brief cache of available remote objects */
        cb_queue        argnotify;  /**< @brief callbacks to notify when remote methods complete */
        iface_map       contracts;  /**< @brief interfaces map of all remote objects */

        registry *reg;      /**< @brief registered objects in session */
        mutex *synchro;     /**< @brief mutex on session manipulation */
        tcp::socket *conn;  /**< @brief connection */

        u32 remoteRoot;     /**< @brief once booted has remote_root */

        gc_map gc_mgr;      /**< @brief tracks dynamically created objects */
    public:
        session();
        virtual ~session();

        /** @brief update interface of specified object */
        void UpdateInterface(u32,string,u32);
        /** @brief gets id of specified object */
        u32 getIdOf(object *ob);
        /** @brief manage object, given object ptr */
        shared get_shared(object*);
        /** @brief manage object, given object id */
        shared get_shared(u32);
        /** @brief delete object, given object ptr */
        void delete_ob(object*);
        /** @brief delete object, given object id */
        void delete_ob(u32);

        /** @brief signals that object no longer valid */
        void notify_remove(u32 id);
        /** @brief close the conncetion */
        void disconnect() {isActive=false;}
        /** @brief register object as available to remote */
        int register_object(object *o);
        /** @brief get remote's root (bootstrap) object */
        u32 getRemote() {return remoteRoot;}
        /** @brief true indicates remote root object valid */
        bool hasRemote() {return 0!=remoteRoot;}
        /** @brief unregister object by-id and inform remote */
        bool unregister(u32 id);
        /** @brief unregister object by-address and inform remote */
        bool unregister(object *ob);
        /** @brief sends any queued values over the wire */
        void encode(const boost::any &a);
        /**
        * @brief Determine type of result stack top value.
        * @throw argstack_empty if the result stack is empty when attempted
        */
        valtype argtype() {
            if (argstack.size()>0) {
                return typeMap[argstack.top().type().name()];
            }
            throw argstack_empty();
        }
        /** @brief current size of result stack */
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
        /** @brief Handshake/bootstrap process */
        void bootstrap(object *root);
        void send_int(s64 val) {sendq.push(val);}               /**< @brief send int to remote */
        //void send_int(s64 &val) {sendq.push(val);}
        void send_float(float val) {sendq.push(val);}           /**< @brief send float to remote */
        //void send_float(float &val) {sendq.push(val);}
        void send_blob(string val) {sendq.push(val);}      /**< @brief send blob (as string) to remote */
        //void send_blob(string &val) {sendq.push(val);}
        void send_string(string val) {sendq.push(val);}    /**< @brief send string to remote */
        //void send_string(string &val) {sendq.push(val);}
        void send_obref(u32 id) {sendq.push(obref(id));}        /**< @brief send object referebce to remote */
        /** @brief Remote method call.
        *
        *   Creates method call message to send to remote.
        *
        *   @param id object id for method
        *   @param m method number
        *   @param cb callback invoked when method completed.
        *          Default: NULL (no callback).
        *   @param rcount expected number of returned values.
        *          Default: 0 (no values returned).
        */
        void send_call(u32 id,u32 m,lpvFunc cb=NULL,int rcount=0) {
            sendq.push(method_call(id,m,cb,rcount));
        }
        void send_call(u32 id,string m_name,lpvFunc cb=NULL,int rcount=0) {
            if (contracts.find(id)==contracts.end())
                throw object_not_reg();
            if (contracts[id].find(m_name)==contracts[id].end())
                throw method_notimpl(m_name,-1);
            sendq.push(method_call(id,contracts[id][m_name],cb,rcount));
        }
        /** @brief Queries if an object still valid  and useable on remote.
        *   @param obid object id.
        *   @return true if oject useable false otherwise.
        */
        bool isValidObject(u32 obid) {
            return (proxy.find(obid)!=proxy.end());
        }
        /** @brief network pump - blocks until data received. */
        bool run();
        /** @brief network pump - returns to allow other activy whilst waiting. */
        bool poll();
        /** @brief get outgoing value queue for this session */
        value_queue &getSendQueue() {return sendq;}
        /** @brief get thread lock for this session */
        mutex &getMutex() {return *synchro;}
        /** @brief associate connection @param s socket to utilize for session */
        void set_conn(tcp::socket &s) {
            conn=&s;
            io_=&(s.get_io_service());
            LOCK_COUT
            cout << "session [" << this << "] on socket " << conn << " via io=" << io_ << endl;
            UNLOCK_COUT
        }
        /** @brief for debugging - dumps data about session */
        void dump(std::ostream &os);
    };

    /**
    ** @brief object reigstry class
    **
    ** registry tracks active object references.
    **
    ** Instances are permitted:
    **
    **      server-side: one instance for each connected client
    **      client-side: one sole instance for upstream server.
    **
    ** @note
    **     Registry does not take ownership of registered
    **     objects instead, object is designed to require
    **     a registry reference at construction.
    */
    class registry {
    private:
        mutex *synchro;     /**< @brief mutex on registry manipulation */
        session *listener;  /**< @brief notify sink for object removals */

        u32 next_slot;          /**< @brief id for next object [must be thread-synced] */
        object_map objects;     /**< @brief registered objects [must be thread-synced] */
    public:
        /** @brief constructor @param host The session reisgtry works for. */
        registry(session *host) : listener(host),next_slot(1)
            {synchro=new mutex();}
        /** @brief destructor */
        virtual ~registry();
        /** @brief add object to registry */
        int register_object(object *ob);
        /** @brief notify upstream of object destruction @param id id of affected object @return allocated slot*/
        void notify(u32 id);
        /** @brief remove object from registry by-id @param id id of object to unregister */
        bool unregister(u32 id);
        /** @brief remove object from registry by-ptr @param ob ptr to object to unregister */
        bool unregister(object *ob);
        /** @brief query id for given object ptr @param ob object ptr @return object id */
        u32 idOf(object* ob);
        /** @brief query ptr for given object id @param id object id @return object ptr */
        object* obOf(u32 id);
        /** @brief for debugging - dumps data about registry */
        void dump(std::ostream &os);
    };

    /** @brief helper typecast symbol for usage of dmc mechanism */
    typedef void(bvnet::object::*dmc)(value_queue&);

    /**
    ** @brief ABC for remotable objects.
    **
    ** Base used for objects exchangeable via object references.
    **
    ** Objects with active references may have methods invoked via
    ** the implemented dispatched method call mechanism.  This system
    ** is OO-friendly and allows runtime polymorphism.  Each object
    ** instance has its own dmc table (think c++ vtable) which may be
    ** be modified at runtime after initial setting by the ctor.  One
    ** use is to swap out initial (compiled) methods for methods invoking
    ** lua scripted forms executed via the (future) embedded lua
    ** interpretor.  In this form you get the javascript-esque object
    ** method redefinition capability.  The other purpose is to allow
    ** proper OO inheritence in derived classes in the heararchy which
    ** may override previous bases' dmc table entries within their ctors.
    **
    ** Since secure referencing requires a way to
    ** track object lifetime a registry reference
    ** is required for construction.
    */
    class object {
    private:
        friend class session;
        call_map dmcTable;  /**< @brief method mapper for method call and OO mechanism */
        name_map dmcLabel;  /**< @brief name labels of dmc methods */
        indx_map dmcIndex;  /**< @brief method indices of dmc method */
        /**
        *   @brief Dispatched Method Call
        *
        *   Implements dispatched method call (dmc)
        *
        *   Superclass-installed dmc methods in dmcTable are
        *   callable by the remote.  A superclass sets up his
        *   dmc methods in his ctor by accessing object's dmcTable
        *
        *   As a plus the value queue boilerplate has been moved
        *   to the base class dispatcher and the dmc methods will
        *   be given the value queue as a parameter.
        *
        *   He also has an exception now to trap calls to an
        *   unimplemented method index.
        */
        void methodCall(unsigned int idx) {
            value_queue &vqueue=ctx.getSendQueue();
            auto dmcFunc=dmcTable.find(idx);
            if (dmcFunc!=dmcTable.end()) {
                (dmcFunc->second)(this,vqueue);
            } else {
                // called a method that doesn't exist
                throw method_notimpl(getType(),idx);
            }
        }
    protected:
        session &ctx;       /**< @brief for objects to attach to the session's registry */

        /** @brief register dmc method @param label method label @param func method function */
        void register_dmc(string label,dmc func) {
            unsigned int slot;
            auto dmcEnt=dmcIndex.find(label);
            if (dmcEnt!=dmcIndex.end()) {
                slot=dmcEnt->second;
            } else {
                slot=dmcIndex.size();
                dmcIndex[label]=slot;
            }
            dmcLabel[slot]=label;
            dmcTable[slot]=func;
            u32 ob=ctx.getIdOf(this);
            ctx.getSendQueue().push((dmc_msg){ob,label,slot});
        }

        const string methodLabel(const unsigned int idx) {
            const auto &s=dmcLabel.find(idx);
            if (s!=dmcLabel.end()) {
                return s->second;
            }
            return std::to_string(idx);
        }

        const unsigned int methodSlot(const string &label) {
            const auto &s=dmcIndex.find(label);
            if (s!=dmcIndex.end()) {
                return s->second;
            }
            throw method_notimpl(getType(),1+dmcIndex.size());
        }

        void dmc_GetType(value_queue&);         /**< @brief the GetType dispatched method call (dmc) */
    public:
        /** @brief construction of an object @param sess reference to session to attach */
        object(session &sess) :
            ctx(sess) {
                LOCK_COUT
                cout << "object [" << this << "] ctor" << endl;
                UNLOCK_COUT
                ctx.register_object(this);
                // dmcTable[0] is the GetType method
                register_dmc("GetType",&object::dmc_GetType);
            }
        /** @brief base dtor to automatically unregister the object */
        virtual ~object() {
            LOCK_COUT
            cout << "object [" << this << "] dtor" << endl;
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
    };

    /*
    **  Object inlines
    */
    inline void object::dmc_GetType(value_queue &vqueue) {
        vqueue.push(string(getType()));
    }

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

        omap_select_addr &table=objects.by<object_addr>();
        omap_iter_byptr victim=table.find(ob);
        if (victim!=table.end()) {
            notify(victim->get<object_id>());
            table.erase(victim);
            return true;
        }
        return false;
    }

    inline u32 registry::idOf(object *ob) {
        /*
        **  get identify (objectref) of pointed object
        */
        if (ob==NULL) throw object_not_reg();   /* indicate DNE if NULL */
        {
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
    inline const char *method_notimpl::what() const throw() {
        snprintf(buf,sizeof(buf),
                 "Method %s.%d not implemented.",
                 dmcOb.c_str(),dmcMethodId);
        return buf;
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
    /** Updates known interface of specifed object
    *   as indicated by the given parameters.
    *
    *   @param ob       [in] object id of interface to update
    *   @param label    [in] name of the new method
    *   @param slot     [in] slot number used to call the new method
    */
    inline void session::UpdateInterface(u32 ob,string label,u32 slot) {
        LOCK_COUT
        cout << "Session [" << this << "] remote obid "
             << ob << " added method " << label
             << " (slot=" << slot << ")" << endl;
        UNLOCK_COUT
        contracts[ob][label]=slot;
    }
    /** @brief Determine registry id given object ptr */
    inline u32 session::getIdOf(object *ob)  {
        return reg->idOf(ob);
    }
    /** @brief manage object, given object ptr
    *   @param p [in] ptr to object
    *   @return object smartptr
    *   @throw object_not_reg if p is not part of session
    */
    inline session::shared session::get_shared(object* p) {
        u32 id=reg->idOf(p);
        if (gc_mgr.find(id)==gc_mgr.end()) {
            gc_mgr[id]=shared(p);
        }
        return gc_mgr[id];
    }
    /** @brief manage object, given object id
    *   @param p [in] object id
    *   @return object smartptr
    *   @throw object_not_reg if id is not part of session
    */
    inline session::shared session::get_shared(u32 id) {
        object *p=reg->obOf(id);
        if (gc_mgr.find(id)==gc_mgr.end()) {
            gc_mgr[id]=shared(p);
        }
        return gc_mgr[id];
    }
    /** @brief delete object, given object ptr
    *   @param p [in] ptr to object
    *   @throw object_not_reg if p is not part of session
    */
    inline void session::delete_ob(object* p) {
        u32 id=reg->idOf(p);
        if (gc_mgr.find(id)!=gc_mgr.end()) {
            gc_mgr.erase(id);
        }
    }
    /** @brief delete object, given object id
    *   @param p [in] object id
    *   @throw object_not_reg if id is not part of session
    */
    inline void session::delete_ob(u32 id) {
        if (gc_mgr.find(id)!=gc_mgr.end()) {
            gc_mgr.erase(id);
        }
    }
    inline void session::notify_remove(u32 id) {
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
    inline void session::on_write_done(string *finishedbuf) {
        delete finishedbuf;
    }
    inline void session::on_write_call_done(string *finishedbuf,lpvFunc cb) {
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
                    cout << "session [" << this
                              << "] booted: remote root=" << idx
                              << endl;
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
                cout << "session [" << this
                          << "] expected objectref got EOF"
                          << " (" << ec << ")"
                          << endl;
                UNLOCK_COUT
                isActive=false;
            }
        }
    }
    inline void session::on_recv_str(const boost::system::error_code &ec,char* buf) {
        if (!ec) {
            argstack.push(string(buf));
            check_argnotify();
        } else {
            LOCK_COUT
            cout << "session [" << this
                      << "] expected string data got EOF"
                      << " (" << ec << ")"
                      << endl;
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
                cout << "session [" << this
                          << "] expected string len got EOF"
                          << " (" << ec << ")"
                          << endl;
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
                /* if there's a contract cached remove it also */
                auto iface=contracts.find(obid);
                if (iface!=contracts.end())
                    contracts.erase(obid);
                LOCK_COUT
                cout << "session [" << this << "] recv ~" << obid << endl;
                UNLOCK_COUT
            } else {
                LOCK_COUT
                cout << "session [" << this
                          << "] expected dead objectid got EOF"
                          << " (" << ec << ")"
                          << endl;
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
                cout << "session [" << this
                          << "] expected callee objectid got EOF"
                          << " (" << ec << ")"
                          << endl;
                UNLOCK_COUT
                isActive=false;
            }
        }
    }
    inline void session::on_recv_dmc_obid(const boost::system::error_code &ec) {
        if (isActive) {
            if (!ec) {
                dmc_msg mk_dmc;
                mk_dmc.ob=*((u32*)in_idx);
                boost::asio::async_read(
                    *conn,
                    boost::asio::buffer(in_idx,4),
                    boost::bind(&session::on_recv_dmc_len,this,
                        boost::asio::placeholders::error,
                        mk_dmc));
            } else {
                LOCK_COUT
                cout << "session [" << this
                          << "] expected dmc objectid got EOF"
                          << " (" << ec << ")"
                          << endl;
                UNLOCK_COUT
                isActive=false;
            }
        }
    }
    inline void session::on_recv_dmc_len(const boost::system::error_code &ec,dmc_msg mk_dmc) {
        if (isActive) {
            if (!ec) {
                u32 idx=*((u32*)in_idx);
                char* pstr=new char[1+idx];
                pstr[idx]='\0';
                boost::asio::async_read(
                    *conn,
                    boost::asio::buffer(pstr,idx),
                    boost::bind(&session::on_recv_dmc_label,this,
                        boost::asio::placeholders::error,
                        mk_dmc,pstr));

            } else {
                LOCK_COUT
                cout << "session [" << this
                          << "] expected dmc objectid got EOF"
                          << " (" << ec << ")"
                          << endl;
                UNLOCK_COUT
                isActive=false;
            }
        }
    }
    inline void session::on_recv_dmc_label(const boost::system::error_code &ec,dmc_msg mk_dmc,char* buf) {
        if (isActive) {
            if (!ec) {
                mk_dmc.label=buf;
                boost::asio::async_read(
                    *conn,
                    boost::asio::buffer(in_idx,4),
                    boost::bind(&session::on_recv_dmc_slot,this,
                        boost::asio::placeholders::error,
                        mk_dmc));

            } else {
                LOCK_COUT
                cout << "session [" << this
                          << "] expected string data got EOF"
                          << " (" << ec << ")"
                          << endl;
                UNLOCK_COUT
                isActive=false;
            }
        }
        delete [] buf;
    }
    inline void session::on_recv_dmc_slot(const boost::system::error_code &ec,dmc_msg mk_dmc) {
        if (isActive) {
            if (!ec) {
                mk_dmc.slot=*((u32*)in_idx);
                contracts[mk_dmc.ob][mk_dmc.label]=mk_dmc.slot;
                LOCK_COUT
                cout << "session [" << this << "] recv dmc ("
                     << mk_dmc.ob << "." << mk_dmc.label
                     << "=" << mk_dmc.slot << ")" << endl;
                UNLOCK_COUT
            } else {
                LOCK_COUT
                cout << "session [" << this
                          << "] expected dmc objectid got EOF"
                          << " (" << ec << ")"
                          << endl;
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
                cout << "Session [" << this << "] call "
                          << ob->getType() << '[' << ob << "]." << ob->methodLabel(idx)
                          << endl;
                UNLOCK_COUT
                ob->methodCall(idx);
            } else {
                LOCK_COUT
                cout << "session [" << this
                          << "] expected method idx got EOF"
                          << " (" << ec << ")"
                          << endl;
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
                cout << "session [" << this
                          << "] expected " << bsize << "-byte int got EOF"
                          << " (" << ec << ")"
                          << endl;
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
                    case ':':
                        boost::asio::async_read(
                            *conn,
                            boost::asio::buffer(in_idx,4),
                            boost::bind(&session::on_recv_dmc_obid,this,
                                boost::asio::placeholders::error));
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
                        cout << "session [" << this << "] recv len="
                                  << rlen << " 0x"
                                  << std::setw(2) << std::setfill('0') << std::hex << ich;
                        if (ich>31 && ich<128)
                            cout << ' ' << in_ch;
                        cout << endl;
                        UNLOCK_COUT
                    }
                }
            } else {
                LOCK_COUT
                if (_float_or_semi) {
                    cout << "session [" << this
                              << "] expected floatnum; got EOF"
                              << " (" << ec << ")"
                              << endl;

                } else {
                    cout << "session [" << this
                              << "] expected opcode got EOF"
                              << " (" << ec << ")"
                              << endl;
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
        } catch (exception &e) {
            isActive=false;
            LOCK_COUT
            cout << "Session [" << this << "] closed: "
                      << e.what() << endl;
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
        } catch (exception &e) {
            isActive=false;
            LOCK_COUT
            cout << "Session " << this << " error: " << e.what()
                      << " (connection closed)" << endl;
            UNLOCK_COUT
            isActive=false;
        }
        return isActive;
    }
    inline void session::encode(const boost::any &raw) {
        std::ostringstream ss;
        u32 idx;
        method_call mc(0,0,NULL,0);
        dmc_msg v_dmc;
        const char *idx_byte=(const char*)&idx;
        type_map::iterator tmi=typeMap.find(raw.type().name());
        if (tmi==typeMap.end()) {
            LOCK_COUT
            cout << "<unknown \"" << raw.type().name() << "\">" << endl;
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
                idx=boost::any_cast<string>(raw).size();
                ss << '"'
                   << idx_byte[0] << idx_byte[1] << idx_byte[2] << idx_byte[3];
                ss << boost::any_cast<string>(raw);
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
            case vtDMC:
                v_dmc=boost::any_cast<dmc_msg>(raw);
                ss << ':';
                idx=v_dmc.ob;
                ss << idx_byte[0] << idx_byte[1] << idx_byte[2] << idx_byte[3];
                idx=v_dmc.label.size();
                ss << idx_byte[0] << idx_byte[1] << idx_byte[2] << idx_byte[3]
                   << v_dmc.label;
                idx=v_dmc.slot;
                ss << idx_byte[0] << idx_byte[1] << idx_byte[2] << idx_byte[3];
                break;
            case vtMethod:
                mc=boost::any_cast<method_call>(raw);
                /*LOCK_COUT
                cout << "call #" << mc.id << '.' << mc.idx
                          << "() cb=" << mc.callbk
                          << ", when stack+=" << mc.rcount
                          << endl;
                UNLOCK_COUT*/
                ss << '.';
                idx=mc.id;
                ss << idx_byte[0] << idx_byte[1] << idx_byte[2] << idx_byte[3];
                idx=mc.idx;
                ss << idx_byte[0] << idx_byte[1] << idx_byte[2] << idx_byte[3];
                break;
            }
            string *dynstr=new string(ss.str());
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
        os << "session object" << endl;
        os << "  argstack count: " << argstack.size() << endl;
        os << "  send queue size: " << sendq.size() << endl;
        os << "  socket: ";
        if (conn==NULL) {
            os << "<none>";
        } else {
            os << conn;
        }
        os << endl;
        UNLOCK_COUT
        reg->dump(os);
    }

    inline void registry::dump(std::ostream &os) {
        LOCK_COUT
        os << "  registry" << endl;
        os << "    objects registered: " << objects.size() << endl;
        object_map::iterator i=objects.begin();
        object *o;
        while (i!=objects.end()) {
            o=i->get<object_addr>();
            os << "      refid=" << i->get<object_id>() << " [";
            os << std::setfill('0') << std::setw(8) << std::hex << (u32)o;
            os << "] type=" << o->getType() << endl;
            ++i;
        }
        UNLOCK_COUT
    }
};  // bvnet

using bvnet::dmc;

#endif // BV_PROTOCOL_HPP_INCLUDED
