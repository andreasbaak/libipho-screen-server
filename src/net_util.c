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

#include "err_util.h"
#include "log_util.h"
#include "net_util.h"

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

int bindServerSocket(const char* portNum, int backlog)
{
    LOG_INFO("Binding server socket to port %s.\n", portNum);

    struct addrinfo hints;
    struct addrinfo* result;
    struct addrinfo* rp;

    int optval;
    int lfd;

    // Call getaddrinfo() to obtain a list of addresses that we can try binding to
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC; // either IPv4 or IPv6
    hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;
    if (getaddrinfo(NULL, portNum, &hints, &result) != 0)
        errExit("getaddrinfo\n");

    // Walk through the list until we find an address to bind to.
    optval = 1;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        lfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (lfd == -1) {
            continue; // On error, try next address
        }
        if (setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
            errExit("setsockopt\n");
        }
        if (bind(lfd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break; // success
        }
        close(lfd);
    }

    if (rp == NULL) {
        errExit("Could not bind socket to any address\n");
    }

    if (listen(lfd, backlog) == -1) {
        errExit("Listen\n");
    }

    freeaddrinfo(result);
    return lfd;
}

void intToByteArray(int integer, char* byteArray)
{
    int i;
    for (i = 0; i < 4; ++i) {
        byteArray[i] = (char) ((integer % 0xff) & 0xff);
        integer /= 0xff;
    }
}

Boolean writeFully(int fd, const char* buffer, size_t length)
{
    ssize_t n;
    const char *p = buffer;
    while (length > 0)
    {
        n = write(fd, p, length);
        if (n == -1) {
            errMsg("write");
            return FALSE;
        }
        p += n;
        length -= n;
    }
    return TRUE;
}
