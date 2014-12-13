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
**  Declaration (header) file client.hpp
**
**  Client defs
**
*/
#ifndef BV_CLIENT_HPP_INCLUDED
#define BV_CLIENT_HPP_INCLUDED

#include "common.hpp"
#include "protocol.hpp"
#include <string>
#include <irrlicht.h>

namespace bvclient {

    using namespace irr;
    using namespace core;
    using namespace scene;
    using namespace video;
    using namespace io;
    using namespace gui;

    class clientRoot : public bvnet::object {
    private:
    protected:
    public:
        clientRoot(bvnet::session &sess)
            : bvnet::object(sess) {
        }
        virtual ~clientRoot() {
            LOCK_COUT
            cout << "clientRoot [" << this << "] gone (via session " << &ctx << ')' << endl;
            UNLOCK_COUT
        }

        virtual const char *getType() {return "clientRoot";}
    };

    class ClientFrontEnd {
    protected:
        IrrlichtDevice* device;
        IVideoDriver* driver;
        IGUIEnvironment* guienv;
        ISceneManager* smgr;
        int textHeight;
    public:
        ClientFrontEnd(IrrlichtDevice* dev) :
            device(dev),
            driver(dev->getVideoDriver()),
            guienv(dev->getGUIEnvironment()),
            smgr(dev->getSceneManager()),
            textHeight(12) {
        }
        ~ClientFrontEnd() {
            device->drop();
        }

        ISceneManager* getSceneManager() {return smgr;}

        bool run() {return device->run();}

        void drawAll() {
            driver->beginScene(true, true, SColor(0,200,200,200));
            smgr->drawAll();
            guienv->drawAll();
            driver->endScene();
        }

        ITexture* getTexture(std::string texfile) {
            return driver->getTexture(texfile.c_str());
        }

        void clearGUI() {
            guienv->clear();
        }

        void setGUIFont(std::string fontfile) {
            IGUISkin* skin = guienv->getSkin();
            IGUIFont* font = guienv->getFont(fontfile.c_str());
            if (font) {
                skin->setFont(font);
                textHeight=(font->getDimension(L"_")).Height;
            }
        }

        bool putGUIMessage(int lines,std::string msg) {
            guienv->clear();
            std::wstring wcvt;
            widen(wcvt,msg);
            guienv->addStaticText(wcvt.data(),
                rect<int>(10,10,410,10+(textHeight*lines)), false);
            if (!device->run()) {
                return false;
            }
            driver->beginScene(true, true, SColor(0,200,200,200));
            smgr->drawAll();
            guienv->drawAll();
            driver->endScene();
            return true;
        }

    };

}

#endif // BV_CLIENT_HPP_INCLUDED
