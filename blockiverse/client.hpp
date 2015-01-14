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
#include "chunk.hpp"
#include <inttypes.h>
#include <string>
#include <irrlicht.h>
#include <map>

namespace bvclient {

    using namespace irr;
    using namespace core;
    using namespace scene;
    using namespace video;
    using namespace io;
    using namespace gui;

    typedef bvmap::Chunk Chunk;
    typedef bvmap::Node Node;
    typedef std::shared_ptr<Chunk> chunkPtr;
    typedef std::map<std::string,chunkPtr> chunkMap;
    typedef std::map<std::string,int> userMap;
    typedef std::map<uint16_t,ITexture*> texMap;

    class chunkCache {
        /** @brief Chunks loaded in memory */
        chunkMap loadedChunks;
        /** @brief How many cells using each chunk
            Remove from loadedChunks and chunkUsers upon reaching 0 */
        userMap  chunkUsers;
        /** @brief 3D chunk cell mapping
            Sized for 16-chunk (256-block) visible range */
        chunkPtr viewRegion[33*33*33];
    };

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
        chunkCache chunks;
        texMap blockTextures;
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
