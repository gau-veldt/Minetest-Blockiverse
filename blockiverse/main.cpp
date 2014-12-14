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
**  Implementation file main.cpp
**
**  Client main driver code.
**
*/
#include "auto/version.h"
#include "common.hpp"
#include <windows.h>
#include <irrlicht.h>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include "rsa/RSA.h"
#include "sha1.hpp"
#include "settings.hpp"
#include "protocol.hpp"
#include <boost/thread/thread.hpp>
#include <boost/filesystem.hpp>
#include <boost/variant.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/uuid/sha1.hpp>
#include "server.hpp"
#include "client.hpp"

using namespace irr;
using namespace core;
using namespace scene;
using namespace video;
using namespace io;
using namespace gui;

/*
**  init: main thread
** write: server thread only
**  read: main thread only
*/
volatile bool serverActive;
/*
**  init: main thread
** write: main thread only
**  read: server thread only
*/
volatile bool req_serverQuit;

void client_default_config(Configurator &cfg) {
    cfg["window_width"]="800";
    cfg["window_height"]="600";
    cfg["driver"]="opengl";
    cfg["standalone"]="1";
    cfg["address"]="localhost";
    cfg["port"]="37001";
    cfg["user"]="singleplayer";
    cfg["passwd"]="";
    /*
    ** 100 is 332-bit RSA key
    ** this already takes several minutes
    ** and the industry is moving to
    ** 2048-bit keys (617)!!! :(
    */
    cfg["key_size"]="32";
}

class ClientEventReceiver : public IEventReceiver {
    virtual bool OnEvent(const SEvent& evt) {
        if (evt.EventType == EET_LOG_TEXT_EVENT) {
            // makes irrlicht's logging messages use output locking
            LOCK_COUT
            cout << evt.LogEvent.Text;
            UNLOCK_COUT
            return true;
        }
    }
};

void testNotify(bvnet::session *s) {
    std::string rc=s->getarg<std::string>();
    LOCK_COUT
    cout << "serverRoot.getType() returned " << rc << endl;
    UNLOCK_COUT
}

void loginDone(bvnet::session *s,bool *authOk,bool *doneFlag) {
    int rc=(int)s->getarg<s64>();
    *authOk=rc;
    *doneFlag=true;
}

void onGetAccount(bvnet::session *s,u32 *acctOb,bool *doneFlag) {
    bvnet::obref acct(0);
    bool loginOK=false;
    try {
        acct=s->getarg<bvnet::obref>();
        loginOK=true;
    } catch (exception &e) {}
    if (loginOK) {
        *acctOb=acct.id;
        LOCK_COUT
        cout << "serverRoot.GetAccount returned objectref id=" << acct.id << endl;
        UNLOCK_COUT
    } else {
        *acctOb=0;
        s64 rc=s->getarg<s64>();
        LOCK_COUT
        cout << "serverRoot.GetAccount returned " << rc << endl;
        UNLOCK_COUT
    }
    *doneFlag=true;
}

void Decrypt(std::string *coded,const Key *key,std::string *uncoded,bool *whenDone) {
    *uncoded=RSA::Decrypt(*coded,*key);
    *whenDone=true;
}

void testLogin(bvnet::session *s,KeyPair *ckey,bool *authOk,bool *doneFlag,bvclient::ClientFrontEnd *fe) {
    boost::thread *worker;
    bool workerDone;

    std::string erc=s->getarg<std::string>();
    std::string rcsha;
    //LOCK_COUT
    //cout << "Decrypting server challenge..." << endl;
    //UNLOCK_COUT
    std::string rc;
    //rc=RSA::Decrypt(erc,ckey->GetPrivateKey());
    fe->putGUIMessage(1,std::string("Authenticating client..."));
    workerDone=false;
    worker=new boost::thread(Decrypt,&erc,&(ckey->GetPrivateKey()),&rc,&workerDone);
    while (!workerDone) {
        if (!fe->run()) {
            worker->join();
        }
    }
    delete worker;
    worker=NULL;

    //LOCK_COUT
    //cout << "    decrypted: " << rc << endl;
    //UNLOCK_COUT
    SHA1 rcdig;
    rcdig.addBytes(rc.c_str(),rc.size());
    unsigned char *dig=rcdig.getDigest();
    std::ostringstream ss;
    for (int i=0;i<20;++i)
        ss << std::setfill('0') << std::setw(2) << std::hex
           << (unsigned int)dig[i];
    rcsha=ss.str();
    free(dig);
    //LOCK_COUT
    //cout << "    SHA1: " << rcsha << endl;
    //UNLOCK_COUT
    s->send_string(rcsha);
    s->send_call(1 /* serverRoot */,"AnswerChallenge",
                 boost::bind(loginDone,s,
                             authOk,doneFlag),
                 1 /* expects one argument */);
}

void DoGenerateKey(bool *whenDone,
                   int keysize,
                   BigInt *priv_mod,
                   BigInt *priv_exp,
                   BigInt *pub_mod,
                   BigInt *pub_exp) {
    LOCK_COUT
    cout << "Generating new client key (size=" << keysize << ")" << endl;
    UNLOCK_COUT
    KeyPair temp_kp=RSA::GenerateKeyPair(keysize);
    *priv_mod=temp_kp.GetPrivateKey().GetModulus();
    *priv_exp=temp_kp.GetPrivateKey().GetExponent();
    *pub_mod=temp_kp.GetPublicKey().GetModulus();
    *pub_exp=temp_kp.GetPublicKey().GetExponent();
    *whenDone=true;
}

using bvclient::clientRoot;
using bvclient::ClientFrontEnd;

int main(int argc, char** argv)
{
    extern void protocol_main_init();
    protocol_main_init();

    boost::thread *server_thread=NULL;
    argset args(argc,argv);

    boost::filesystem::path cwd=boost::filesystem::current_path();
    boost::filesystem::path media=cwd/".."/"client";
    boost::filesystem::path fonts=media/"fonts";

    Configurator config((cwd/"client.cfg").string(),client_default_config);
    config.read_cmdline(argc,argv);

    std::string ver=auto_ver;

    LOCK_COUT
    cout << "Client version is: " << ver << endl;
    cout << "Client starting in: " << cwd << endl;
    /* test settings map */
    /*for (auto &setting : config) {
        cout << setting.first << "="
            << setting.second << endl;
    }*/
    UNLOCK_COUT

    bool taskIsDone=false;
    boost::thread* task_Worker=NULL;

    E_DRIVER_TYPE vdrv=EDT_SOFTWARE;
    if (v2str(config["driver"])=="opengl") vdrv=EDT_OPENGL;

    int winW,winH;
    winW=v2int(config["window_width"]);
    winH=v2int(config["window_height"]);
    IrrlichtDevice *device =
        createDevice(vdrv, dimension2d<u32>(winW, winH), 16,
            false, false, false, 0);
    device->setWindowCaption(L"Blockiverse client");
    ClientFrontEnd FrontEnd(device);

    FrontEnd.setGUIFont((fonts/"fontlucida.png").string());

    if (!FrontEnd.putGUIMessage(1,std::string(
        "Blockiverse version ")+ver+" starting up...")) {
        return 0;
    }

    /*
    ** start server when running standalone
    ** so there will be something to connect to
    */
    bool standalone=(0!=v2int(config["standalone"]));
    if (standalone) {
        LOCK_COUT
        cout << "Starting server." << endl;
        UNLOCK_COUT
        serverActive=true;
        req_serverQuit=false;
        server_thread=new boost::thread(server_main,&args);
    }

    /*
    ** load client keypair
    ** or generate if they do not exist
    */
    BigInt pub_mod,pub_exp,priv_mod,priv_exp;
    KeyPair *client_kpair=NULL;
    std::string s;
    LOCK_COUT
    cout << "Loading client keys..." << endl;
    UNLOCK_COUT
    try {
        std::ifstream keyfile;
        keyfile.exceptions(std::ios::failbit | std::ios::badbit);
        keyfile.open((cwd/"client.keys").string(),std::ios::in);
        keyfile >> s;
        priv_mod=BigInt(s);
        keyfile >> s;
        priv_exp=BigInt(s);
        keyfile >> s;
        pub_mod=BigInt(s);
        keyfile >> s;
        pub_exp=BigInt(s);
        client_kpair=new KeyPair(Key(priv_mod,priv_exp),Key(pub_mod,pub_exp));
    } catch (exception &e) {
        LOCK_COUT
        cout << "Failed to read keyfile: " << e.what() << endl;
        UNLOCK_COUT
    }
    if (client_kpair==NULL) {

        if (!FrontEnd.putGUIMessage(3,std::string(
                "Generating new client key.\n"
                "    This is only done once after installation,\n"
                "    but may take a few minutes..."))) {
            return 0;
        }
        task_Worker=new boost::thread(DoGenerateKey,&taskIsDone,v2int(config["key_size"]),&priv_mod,&priv_exp,&pub_mod,&pub_exp);
        while (!taskIsDone) {
            if (!device->run()) {
                delete task_Worker;
                return 0;
            }
        }
        task_Worker->join();
        delete task_Worker;
        task_Worker=NULL;

        client_kpair=new KeyPair(Key(priv_mod,priv_exp),Key(pub_mod,pub_exp));
        LOCK_COUT
        cout << "New client key generated." << endl;
        UNLOCK_COUT
        try {
            // write key
            std::ofstream keyfile;
            keyfile.exceptions(std::ios::failbit | std::ios::badbit);
            keyfile.open((cwd/"client.keys").string(),std::ios::out|std::ios::trunc);
            keyfile << priv_mod << endl;
            keyfile << priv_exp << endl;
            keyfile << pub_mod << endl;
            keyfile << pub_exp << endl;
            keyfile.close();
        } catch (exception &e) {
            LOCK_COUT
            cout << "Failed to write keyfile: " << e.what() << endl;
            UNLOCK_COUT
        }
    }
    //LOCK_COUT
    //cout << *client_kpair << endl;
    //UNLOCK_COUT

    if (!FrontEnd.putGUIMessage(1,std::string("Connecting to ")
        +config["address"]+":"+config["port"]
        +" as "+config["user"]+"...")) {
        return 0;
    }

    /*
    ** create stuff client needs such as
    ** his session object and clientRoot
    */
    bvnet::session client_session;
    LOCK_COUT
    cout << "client session ["
              << &client_session << "]" << endl;;
    UNLOCK_COUT
    clientRoot client_root(client_session);
    int port=v2int(config["port"]);
    std::ostringstream s_port;
    s_port << port;
    std::string host=v2str(config["address"]);
    io_service client_io;
    tcp::resolver resolv(client_io);
    tcp::resolver::query lookup(host,s_port.str());
    tcp::resolver::iterator target=resolv.resolve(lookup);
    tcp::socket socket(client_io);
    LOCK_COUT
    cout << "Connecting to "
              << host << " port " << port << endl;
    UNLOCK_COUT
    boost::asio::connect(socket,target);
    client_session.set_conn(socket);
    LOCK_COUT
    cout << "Client connected." << endl;
    UNLOCK_COUT
    //client_session.dump(cout);
    client_session.bootstrap(&client_root);
    while (client_session.run() && !client_session.hasRemote()) {
        /* until session dies or remote root is known  */
    }

    if (client_session.hasRemote()) {
        LOCK_COUT
        cout << "client's serverRoot=" << client_session.getRemote() << endl;
        UNLOCK_COUT

        /*
        **  Test method call mechanism
        */
        client_session.send_call(1 /* serverRoot */,"GetType",
                                 boost::bind(testNotify,&client_session),
                                 1 /* expects one argument */);

        bool authOk=false,authDone=false;
        u32 acctId=0;
        client_session.send_string(client_kpair->GetPublicKey().GetModulus());
        client_session.send_string(client_kpair->GetPublicKey().GetExponent());
        client_session.send_call(1 /* serverRoot */,"LoginClient",
                                 boost::bind(testLogin,&client_session,
                                             client_kpair,&authOk,&authDone,&FrontEnd),
                                 1 /* expects one result */);

        while (!authDone
               && client_session.run()
               && FrontEnd.run());
        if (!FrontEnd.run()) {
            return 0;
        }
        LOCK_COUT
        if (authOk) {
            cout << "Server accepted client auth." << endl;
        } else {
            cout << "Server rejected client auth." << endl;
        }
        UNLOCK_COUT

        FrontEnd.putGUIMessage(1,std::string("Logging in..."));
        std::string userName=config["user"];
        std::string userPass=config["passwd"];
        if (authOk) {
            // username
            client_session.send_string(userName);
            // password
            client_session.send_string(RSA::Encrypt(userPass,client_kpair->GetPrivateKey()));
            authDone=false;
            client_session.send_call(1 /* serverRoot */,"GetAccount",
                                     boost::bind(onGetAccount,&client_session,
                                                 &acctId,&authDone),
                                     1 /* expects one result */);
            while (!authDone && client_session.run());
        }
        LOCK_COUT
        if (acctId>0) {
            cout << "Logged in as " << userName
                      << " with account object id=" << acctId << endl;
        } else {
            cout << "Inavlid credntials for user " << userName << endl;
        }
        UNLOCK_COUT

        if (acctId>0){
            FrontEnd.clearGUI();

            if (vdrv==EDT_OPENGL) {
                FrontEnd.putGUIMessage(1,"Hello World! This is the Irrlicht OpenGL renderer!");
            } else {
                FrontEnd.putGUIMessage(1,"Hello World! This is the Irrlicht Software renderer!");
            }

            ISceneManager* smgr=FrontEnd.getSceneManager();

            /*
            To display something interesting, we load a Quake 2 model
            and display it. We only have to get the Mesh from the Scene
            Manager (getMesh()) and add a SceneNode to display the mesh.
            (addAnimatedMeshSceneNode()). Instead of writing the filename
            sydney.md2, it would also be possible to load a Maya object file
            (.obj), a complete Quake3 map (.bsp) or a Milshape file (.ms3d).
            By the way, that cool Quake 2 model called sydney was modelled
            by Brian Collins.
            */
            IAnimatedMesh* mesh = smgr->getMesh("../../media/sydney.md2");
            IAnimatedMeshSceneNode* node = smgr->addAnimatedMeshSceneNode( mesh );

            /*
            To let the mesh look a little bit nicer, we change its material a
            little bit: We disable lighting because we do not have a dynamic light
            in here, and the mesh would be totally black. Then we set the frame
            loop, so that the animation is looped between the frames 0 and 310.
            And at last, we apply a texture to the mesh. Without it the mesh
            would be drawn using only a color.
            */
            if (node)
            {
                node->setMaterialFlag(EMF_LIGHTING, false);
                node->setFrameLoop(0, 310);
                node->setMaterialTexture(0, FrontEnd.getTexture("../../media/sydney.bmp"));
            }

            /*
            To look at the mesh, we place a camera into 3d space at the position
            (0, 30, -40). The camera looks from there to (0,5,0).
            */
            smgr->addCameraSceneNode(0, vector3df(0,30,-40), vector3df(0,5,0));

            /*
            Ok, now we have set up the scene, lets draw everything:
            We run the device in a while() loop, until the device does not
            want to run any more. This would be when the user closed the window
            or pressed ALT+F4 in windows.
            */
            while(FrontEnd.run()
                && client_session.poll() /* client still connected */) {
                FrontEnd.drawAll();
            }
        }
    }

    // close connection to server
    LOCK_COUT
    cout << "Closing connection to server." << endl;
    UNLOCK_COUT
    socket.close();

    if (standalone) {
        // defibrilates main server thread if blocked
        LOCK_COUT
        cout << "Requesting server quit." << endl;
        UNLOCK_COUT
        req_serverQuit=true;
        boost::asio::connect(socket,target);
        socket.close();

        LOCK_COUT
        cout << "Waiting for server shutdown." << endl;
        UNLOCK_COUT
        if (server_thread!=NULL) {
            server_thread->join();  //while (serverActive) ;
            delete server_thread;
            server_thread=NULL;
        }
    }

    return 0;
}
