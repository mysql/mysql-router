/*
  Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef UTILS_ROUTING_INCLUDED
#define UTILS_ROUTING_INCLUDED

#include <array>
#include <iostream>
#include <sstream>
#include <vector>
#ifndef _WIN32
# include <netinet/in.h>
# include <netdb.h>
# include <unistd.h>
#else
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <winsock2.h>
# include <ws2tcpip.h>
# include <stdint.h>
#endif

/**
 * Socket address from either IPv4 or IPv6
 *
 * @param addr addrinfo struct
 * @return struct in_addr
 */
void * get_in_addr(struct sockaddr *addr);

/**
 * Get address of connected peer
 *
 * Get address of peer connected to the specified
 * socket. This works similar as getpeername() but will handle
 * IPv4, IPv6 and Unix sockets/Windows named pipes.
 *
 * @param int socket
 * @return std::pair with std::string and uint16_t
 */
std::pair<std::string, int > get_peer_name(int sock);

/**
 * Splits a string using a delimiter
 *
 * @param data a string to split
 * @param delimiter a char used as delimiter
 * @param bool whether to allow empty tokens or not (default true)
 * @return std::vector<string> containing tokens
 */
std::vector<std::string> split_string(const std::string& data, const char delimiter, bool allow_empty);

/** @overload */
std::vector<std::string> split_string(const std::string& data, const char delimiter);

/** @brief Converts IP addr to std::array
 *
 * Converts a IP address stored in a sockaddr_storage struct to a
 * std::array of size 16.
 *
 * @param addr a sockaddr_storage struct
 * @return std::array<uint8_t, 16>
 */
std::array<uint8_t, 16> in_addr_to_array(const sockaddr_storage& addr);

std::string get_message_error(int errcode);

#endif // UTILS_ROUTING_INCLUDED
