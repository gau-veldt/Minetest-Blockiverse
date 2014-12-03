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

using boost::is_any_of;
using boost::token_compress_on;
using boost::filesystem::path;
using boost::split;

Configurator::Configurator(const string &cfgPath) {
    cfgFile=cfgPath;
    read();
}

void Configurator::process_line(const string &line) {
    std::vector<string> parts;
    split(parts,line,boost::is_any_of("="),boost::token_compress_on);
    if (parts.size()>1) {
        process_clause(parts[0],parts[1]);
    }
}

void Configurator::process_clause(const string &key,const string &val) {
    LOCK_COUT
    cout << "[conf] " << path(cfgFile).filename().string()
              << ": " << key << "=" << val << endl;
    UNLOCK_COUT
    cfg[key]=val;
}

void Configurator::read_cmdline(int argc,char **argv) {
    for (int i=0;i<argc;++i) {
        /*LOCK_COUT
        cout << "from cmdline: " << line << endl;
        UNLOCK_COUT*/
        process_line(argv[i]);
    }
}

void Configurator::read() {
    string line;
    std::ifstream in(cfgFile);
    std::getline(in,line);
    while (in) {
        /*LOCK_COUT
        cout << "from " << cfgFile << ": " << line << endl;
        UNLOCK_COUT*/
        process_line(line);
        std::getline(in,line);
    }
}

void Configurator::write() {
    std::ofstream out(cfgFile);
    for (auto &each : cfg) {
        out << each.first << "=" << each.second << endl;
    }
}
