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
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "protocol.hpp"
#include <boost/variant.hpp>
#include <boost/lexical_cast.hpp>
#include "server.hpp"
#include "client.hpp"
#include "settings.hpp"

/*
** meters per parsec
*/
u64 m_per_parsec=30856775800000000;

/*
In the Irrlicht Engine, everything can be found in the namespace
'irr'. So if you want to use a class of the engine, you have to
write an irr:: before the name of the class. For example to use
the IrrlichtDevice write: irr::IrrlichtDevice. To get rid of the
irr:: in front of the name of every class, we tell the compiler
that we use that namespace from now on, and we will not have to
write that 'irr::'.
*/
using namespace irr;

/*
There are 5 sub namespaces in the Irrlicht Engine. Take a look
at them, you can read a detailed description of them in the
documentation by clicking on the top menu item 'Namespace List'
or using this link: http://irrlicht.sourceforge.net/docu/namespaces.html.
Like the irr Namespace, we do not want these 5 sub namespaces now,
to keep this example simple. Hence we tell the compiler again
that we do not want always to write their names:
*/
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

class ClientEventReceiver : public IEventReceiver {
    virtual bool OnEvent(const SEvent& evt) {
        if (evt.EventType == EET_LOG_TEXT_EVENT) {
            // makes irrlicht's logging messages use output locking
            LOCK_COUT
            std::cout << evt.LogEvent.Text;
            UNLOCK_COUT
            return true;
        }
    }
};

int main(int argc, char** argv)
{
    extern void protocol_main_init();
    protocol_main_init();

    DWORD server_tid=0;
    HANDLE server_thread=NULL;
    argset args(argc,argv);
    property_map config;
    set_config_defaults(config);
    std::string cfg_opt;

    LOCK_COUT
    std::cout << "Version is: " << auto_ver << std::endl;

    /* test settings map */
    property_map::iterator setting=config.begin();
    while (setting!=config.end()) {
        std::cout << setting->first << "="
            << setting->second << std::endl;
        ++setting;
    }
    UNLOCK_COUT

    /*
    ** start server when running standalone
    ** so there will be something to connect to
    */
    bool standalone=(0!=v2int(config["standalone"]));
    if (standalone) {
        LOCK_COUT
        std::cout << "Starting server." << std::endl;
        UNLOCK_COUT
        serverActive=true;
        req_serverQuit=false;
        // I'm giving up getting this to work portably via boost for now
        // until they fix the crap with _InterlockedCompareExchange
        // and the consequent linker errors
        server_thread=CreateThread(
            NULL,           /* default security*/
            0,              /* default stack */
            &server_main,   /* entry point */
            &args,          /* struct to pass args */
            0,              /* default creation */
            &server_tid);   /* where to store thread id */
    }

    /*
    ** create stuff client needs such as
    ** his session object and clientRoot
    */
    bvnet::session client_session;
    LOCK_COUT
    std::cout << "client session [" << &client_session << "]";
    UNLOCK_COUT
    clientRoot client_root(client_session);
    int port=v2int(config["port"]);
    std::ostringstream s_port;
    s_port << port;
    std::string host=v2str(config["address"]);
    io_service io;
    tcp::resolver resolv(io);
    tcp::resolver::query lookup(host,s_port.str());
    tcp::resolver::iterator target=resolv.resolve(lookup);
    tcp::socket socket(io);
    LOCK_COUT
    std::cout << "Connecting to " << host << " port " << port << std::endl;
    UNLOCK_COUT
    boost::asio::connect(socket,target);
    client_session.set_conn(socket);
    LOCK_COUT
    std::cout << "Client connected." << std::endl;
    UNLOCK_COUT
    //client_session.dump(std::cout);
    client_session.bootstrap(&client_root);
    while (client_session.run() && !client_session.hasRemote()) {
        /* until session dies or remote root is known  */
    }

    if (client_session.hasRemote()) {
        LOCK_COUT
        std::cout << "client's serverRoot=" << client_session.getRemote() << std::endl;
        UNLOCK_COUT

        /*
        The most important function of the engine is the 'createDevice'
        function. The Irrlicht Device can be created with it, which is the
        root object for doing everything with the engine.
        createDevice() has 7 paramters:
        deviceType: Type of the device. This can currently be the Null-device,
           the Software device, DirectX8, DirectX9, or OpenGL. In this example we use
           EDT_SOFTWARE, but to try out, you might want to change it to
           EDT_NULL, EDT_DIRECTX8 , EDT_DIRECTX9, or EDT_OPENGL.
        windowSize: Size of the Window or FullscreenMode to be created. In this
           example we use 640x480.
        bits: Amount of bits per pixel when in fullscreen mode. This should
           be 16 or 32. This parameter is ignored when running in windowed mode.
        fullscreen: Specifies if we want the device to run in fullscreen mode
           or not.
        stencilbuffer: Specifies if we want to use the stencil buffer for drawing shadows.
        vsync: Specifies if we want to have vsync enabled, this is only useful in fullscreen
          mode.
        eventReceiver: An object to receive events. We do not want to use this
           parameter here, and set it to 0.
        */

        E_DRIVER_TYPE vdrv=EDT_SOFTWARE;
        cfg_opt=v2str(config["driver"]);
        if (cfg_opt=="opengl") vdrv=EDT_OPENGL;

        int winW,winH;
        winW=v2int(config["window_width"]);
        winH=v2int(config["window_height"]);
        IrrlichtDevice *device =
            createDevice(vdrv, dimension2d<u32>(winW, winH), 16,
                false, false, false, 0);

        /*
        Set the caption of the window to some nice text. Note that there is
        a 'L' in front of the string. The Irrlicht Engine uses wide character
        strings when displaying text.
        */
        device->setWindowCaption(L"Hello World! - Irrlicht Engine Demo");

        /*
        Get a pointer to the video driver, the SceneManager and the
        graphical user interface environment, so that
        we do not always have to write device->getVideoDriver(),
        device->getSceneManager() and device->getGUIEnvironment().
        */
        IVideoDriver* driver = device->getVideoDriver();
        ISceneManager* smgr = device->getSceneManager();
        IGUIEnvironment* guienv = device->getGUIEnvironment();

        /*
        We add a hello world label to the window, using the GUI environment.
        */
        const wchar_t *helloText=L"Hello World! This is the Irrlicht Software renderer!";
        if (vdrv==EDT_OPENGL)
            helloText=L"Hello World! This is the Irrlicht OpenGL renderer!";
        guienv->addStaticText(helloText,
            rect<int>(10,10,200,22), true);

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
            node->setMaterialTexture( 0, driver->getTexture("../../media/sydney.bmp") );
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
        while(device->run() && client_session.poll() /* client still connected */)
        {
            if (client_session.argcount()>0) {
                LOCK_COUT
                bvnet::valtype vt=client_session.argtype();
                switch (vt) {
                case bvnet::vtInt:
                    std::cout << "Client got int: "
                              << client_session.getarg<s64>()
                              << std::endl;
                    break;
                case bvnet::vtFloat:
                    std::cout << "Client got float: "
                              << client_session.getarg<float>()
                              << std::endl;
                    break;
                case bvnet::vtBlob:
                case bvnet::vtString:
                    std::cout << "Client got string/blob: "
                              << client_session.getarg<std::string>()
                              << std::endl;
                    break;
                case bvnet::vtObref:
                    std::cout << "Client got objectref id="
                              << client_session.getarg<bvnet::obref>().id
                              << std::endl;
                    break;
                case bvnet::vtDeath:
                    std::cout << "Client's objectref #"
                              << client_session.getarg<bvnet::ob_is_gone>().id
                              << " is dead" << std::endl;
                    break;
                }
                UNLOCK_COUT
            }

            /*
            Anything can be drawn between a beginScene() and an endScene()
            call. The beginScene clears the screen with a color and also the
            depth buffer if wanted. Then we let the Scene Manager and the
            GUI Environment draw their content. With the endScene() call
            everything is presented on the screen.
            */
            driver->beginScene(true, true, SColor(0,200,200,200));

            smgr->drawAll();
            guienv->drawAll();

            driver->endScene();
        }

        /*
        After we are finished, we have to delete the Irrlicht Device
        created before with createDevice(). In the Irrlicht Engine,
        you will have to delete all objects you created with a method or
        function which starts with 'create'. The object is simply deleted
        by calling ->drop().
        See the documentation at
        http://irrlicht.sourceforge.net//docu/classirr_1_1IUnknown.html#a3
        for more information.
        */
        device->drop();

    }

    // close connection to server
    LOCK_COUT
    std::cout << "Closing connection to server." << std::endl;
    UNLOCK_COUT
    socket.close();

    // defibrilates if server in blocking accept
    LOCK_COUT
    std::cout << "Requesting server quit." << std::endl;
    UNLOCK_COUT
    req_serverQuit=true;
    boost::asio::connect(socket,target);
    socket.close();

    LOCK_COUT
    std::cout << "Waiting for server shutdown." << std::endl;
    UNLOCK_COUT
    while (serverActive) ;

    if (server_thread!=NULL) {
        // kills server thread
        DWORD rc;
        GetExitCodeThread(server_thread,&rc);
        if (rc==STILL_ACTIVE) {
            /*
            ** TODO: This is the ugly and dangerous way to stop
            **       the server and needs a cleaner alternative!
            */
            TerminateThread(server_thread,9/* SIGKILL */);
        }
        // then close handle
        CloseHandle(server_thread);
        server_thread=NULL;
    }

    return 0;
}

