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
#include <exception>
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

extern int log2_tbl[];
extern int pow2_tbl[];

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
        vtDeath=6,
        vtMethod=7
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
    struct method_call {;
        /* wrapper for method call message */
        u32 id;
        u32 idx;
        method_call(u32 i,u32 m):id(i),idx(m) {}
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
    typedef std::stack<boost::any> value_stack;
    typedef std::queue<boost::any> value_queue;
    typedef std::map<u32,bool> proxy_map;
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

    class session {
    private:
        io_service *io_;
        object *root;
        bool isActive;
        bool isBooting;
        bool _float_or_semi;
        bool _neg_int;
        bool _opcode_read_queued;
        std::string _fpstr;
        char in_ch;
        char in_idx[4];
        char in_s64[8];
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

        /* value stack */
        value_stack argstack;
        value_queue sendq;
        proxy_map   proxy;

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
        valtype argtype() {
            if (argstack.size()>0) {
                return typeMap[argstack.top().type().name()];
            }
            throw argstack_empty();
        }
        int argcount() {return argstack.size();}
        template<typename V>
        V getarg() {
            if (argstack.size()>0) {
                V rc=boost::any_cast<V>(argstack.top());
                argstack.pop();
                return rc;
            }
            throw argstack_empty();
        }
        void bootstrap(object *root);
        void send_int(s64 val) {sendq.push(val);}
        void send_int(s64 &val) {sendq.push(val);}
        void send_float(float val) {sendq.push(val);}
        void send_float(float &val) {sendq.push(val);}
        void send_blob(std::string val) {sendq.push(val);}
        void send_blob(std::string &val) {sendq.push(val);}
        void send_string(std::string val) {sendq.push(val);}
        void send_string(std::string &val) {sendq.push(val);}
        void send_obref(u32 id) {sendq.push(obref(id));}
        void send_call(u32 id,u32 m) {sendq.push(method_call(id,m));}
        bool isValidObject(u32 obid) {
            return (proxy.find(obid)!=proxy.end());
        }
        bool run();
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
                ss << '.';
                idx=boost::any_cast<method_call>(raw).id;
                ss << idx_byte[0] << idx_byte[1] << idx_byte[2] << idx_byte[3];
                idx=boost::any_cast<method_call>(raw).idx;
                ss << idx_byte[0] << idx_byte[1] << idx_byte[2] << idx_byte[3];
                break;
            }
            std::string *dynstr=new std::string(ss.str());
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
