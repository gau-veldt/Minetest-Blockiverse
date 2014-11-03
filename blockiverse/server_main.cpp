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
**  Implementation file server_main.cpp
**
**  Server main driver code.
**
*/

#include "common.hpp"
#include <iostream>
#include "sqlite/sqlite3.h"

int main(int argc, char** argv)
{
    std::cout << "Version is: " << auto_ver << std::endl;

    std::cout << std::endl << "Press enter to continue." << std::endl;
    std::cin.ignore();

    return 0;
}

