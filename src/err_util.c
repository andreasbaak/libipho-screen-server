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


#include "ename.c.inc"
#include "err_util.h"

#include <stdio.h>
#include <stdlib.h>

void errExit(const char* msg)
{
    fprintf(stderr, "[%s %s] %s\n", (errno > 0) ? ename[errno] : "?UNKNOWN?", strerror(errno), msg);
    exit(1);
}

void errExitEN(int en, const char* msg)
{
    fprintf(stderr, "[%s %s] %s\n", (en > 0) ? ename[en] : "?UNKNOWN?", strerror(en), msg);
    exit(1);
}

void errMsg(const char* msg)
{
    int savedErrno;
    savedErrno = errno;
    fprintf(stderr, "[%s %s] %s\n", (errno > 0) ? ename[errno] : "?UNKNOWN?", strerror(errno), msg);
    errno = savedErrno;
}

