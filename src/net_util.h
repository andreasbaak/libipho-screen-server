/*
libipho-screen-server is a relay server for photobooth data.

Copyright (C) 2015 Andreas Baak (andreas.baak@gmail.com)

This file is part of libipho-screen-server.

libipho-screen-server is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

libipho-screen-server is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with libipho-screen-server. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef NET_UTIL_H_
#define NET_UTIL_H_

#include "boolean_util.h"

/**
 * Create a server socket at any available host interface on the provided port.
 * Bind a file descriptor to it and put it into into listen mode.
 * Return the file descriptor that can be used to accept client connections.
 *
 * \param portNum
 * String that represent the port number that the server socket should bind to
 * \param backlog
 * Maximal length of the queue of pending connections, see <man listen>.
 * \return
 * File descriptor corresponding to the server socket in listen mode.
 */
int bindServerSocket(const char* portNum, int backlog);

/**
 * Converts an integer into a byte array of 4 bytes so that
 * the byte array representation is independent of the
 * byte order of the host system.
 *
 * The caller has to provide a pointer byteArray that
 * points to at lease 4 bytes of memory.
 * byteArray will be filled as follows:
 *
 * byteArray[0] = Least significant 8 bits
 * byteArray[1] = ..
 * byteArray[2] = ..
 * byteArray[3] = Most significant 8 bits
 *
 * \param integer
 * The integer to convert
 * \param byteArray
 * At least 4 bytes of allocated memory. This memory area
 * will contain the representation of the integer.
 */
void intToByteArray(int integer, char* byteArray);


/**
 * Send length bytes of the provided buffer on the provided file descriptor
 * by repeatedly probing the write function until either
 * the whole length bytes have been written or an error occured.
 *
 * \return
 * TRUE if all bytes have been sent, FALSE otherwise.
 */
Boolean writeFully(int fd, const char* buffer, size_t length);
#endif

