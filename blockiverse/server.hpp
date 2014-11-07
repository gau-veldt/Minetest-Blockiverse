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
**  Declaration (header) file server.hpp
**
**  declarations for server
**
*/
#ifndef BV_SERVER_HPP_INCLUDED
#define BV_SERVER_HPP_INCLUDED

#ifdef BV_SERVER_IMPLEMENTATION
/*
** server internal header only visible to server implementation
*/

#else
/*
** make server startup visible only
*/

extern int server_main(int argc, char** argv);

#endif // BV_SERVER_IMPLEMENTATION
#endif // BV_SERVER_HPP_INCLUDED
