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

#include <boost/lexical_cast.hpp>
#include <map>

typedef std::map<std::string,std::string> property_map;

#define v2int boost::lexical_cast<int>
#define v2str boost::lexical_cast<std::string>
#define v2flt boost::lexical_cast<float>

class Configurator {
private:
    typedef property_map::key_type cfg_key;
    typedef property_map::mapped_type cfg_val;
    property_map cfg;
    std::string cfgFile;
    void read();
    void write();
    void process_line(const std::string &);
protected:
    virtual void process_clause(const std::string &,const std::string &);
public:
    Configurator(const std::string &);
    ~Configurator() {try {write();} catch (std::exception &e) {}}

    void read_cmdline(int,char**);

    typedef property_map::iterator iterator;
    iterator begin() {return cfg.begin();}
    iterator end() {return cfg.end();}
    cfg_val &operator [](const cfg_key &id) {return cfg[id];}
};

#endif // BV_SETTINGS_HPP_INCLUDED
