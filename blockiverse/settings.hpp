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
**  Declaration (header) file settings.hpp
**
**  Handles settings/config file (header)
**
*/
#ifndef BV_SETTINGS_HPP_INCLUDED
#define BV_SETTINGS_HPP_INCLUDED

#include <boost/variant.hpp>
#include <map>

typedef boost::variant<int,float,std::string> setting_t;
typedef std::map<std::string,setting_t> property_map;
#define v2str boost::get<std::string>
#define v2flt boost::get<float>
#define v2int boost::get<int>

extern void set_config_defaults(property_map &cfg);

#endif // BV_SETTINGS_HPP_INCLUDED
