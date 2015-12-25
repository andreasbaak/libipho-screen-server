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
