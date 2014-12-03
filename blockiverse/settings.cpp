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
#include "common.hpp"
#include "settings.hpp"
#include <fstream>
#include <vector>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

Configurator::Configurator(const std::string &cfgPath) {
    cfgFile=cfgPath;
    read();
}

void Configurator::process_line(const std::string &line) {
    std::vector<std::string> parts;
    boost::split(parts,line,boost::is_any_of("="),boost::token_compress_on);
    if (parts.size()>1) {
        process_clause(parts[0],parts[1]);
    }
}

void Configurator::process_clause(const std::string &key,const std::string &val) {
    LOCK_COUT
    std::cout << "[conf] " << boost::filesystem::path(cfgFile).filename()
              << ": " << key << "=" << val << std::endl;
    UNLOCK_COUT
    cfg[key]=val;
}

void Configurator::read_cmdline(int argc,char **argv) {
    for (int i=0;i<argc;++i) {
        /*LOCK_COUT
        std::cout << "from cmdline: " << line << std::endl;
        UNLOCK_COUT*/
        process_line(argv[i]);
    }
}

void Configurator::read() {
    std::string line;
    std::ifstream in(cfgFile);
    std::getline(in,line);
    while (in) {
        /*LOCK_COUT
        std::cout << "from " << cfgFile << ": " << line << std::endl;
        UNLOCK_COUT*/
        process_line(line);
        std::getline(in,line);
    }
}

void Configurator::write() {
    std::ofstream out(cfgFile);
    for (auto &each : cfg) {
        out << each.first << "=" << each.second << std::endl;
    }
}
