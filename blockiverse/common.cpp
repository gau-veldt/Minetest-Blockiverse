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
**  Implementation file common.cpp
**
**  Common implementation
**
*/
#include "common.hpp"

/** @brief meters per parsec */
u64 m_per_parsec=30856775800000000;

int widen(std::wstring &wide,const std::string &narrow) {
    std::wstring ws(narrow.size(), L' '); // Overestimate number of code points.
    ws.resize(mbstowcs(&ws[0], narrow.c_str(), narrow.size())); // Shrink to fit.
    wide=ws;
    return 0;
}
