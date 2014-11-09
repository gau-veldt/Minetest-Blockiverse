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
**  Implementation file settings.cpp
**
**  config/settings implementation
**
*/
#include "settings.hpp"

void set_config_defaults(property_map &cfg) {
    cfg["window_width"]=800;
    cfg["window_height"]=600;
    cfg["driver"]="opengl";
    cfg["standalone"]=1;
    cfg["address"]="localhost";
    cfg["port"]=37001;
}
