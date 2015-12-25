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

#ifndef ERR_UTIL_H_
#define ERR_UTIL_H_

#include <errno.h>
#include <string.h>

void errExit(const char* msg);
void errExitEN(int en, const char* msg);
void errMsg(const char* msg);

#endif
