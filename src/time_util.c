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

#include "time_util.h"

struct timespec timespecAdd(const struct timespec  a, const struct timespec  b) {
    struct timespec res;
    res.tv_sec = a.tv_sec + b.tv_sec;
    res.tv_nsec = a.tv_nsec + b.tv_nsec;
    long carry = res.tv_nsec / 1e9;
    res.tv_sec += carry;
    res.tv_nsec = res.tv_nsec - (carry*1e9);
    return res;
} 

struct timespec computeAbsoluteTimeout(long deltaNanos) {
    struct timespec  now;
    struct timespec  diff;
    clock_gettime(CLOCK_REALTIME, &now);
    diff.tv_sec = 0;
    diff.tv_nsec = deltaNanos;
    return timespecAdd(now, diff);
}
