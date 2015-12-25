#ifndef TIME_UTIL_H
#define TIME_UTIL_H
#include <time.h>

/**
 * Add two timespec structs.
 * Return a+b
 */
struct timespec timespecAdd(const struct timespec a, const struct timespec b);

/**
 * Return a timespec that corresponds to NOW + deltaNanos
 */
struct timespec computeAbsoluteTimeout(long deltaNanos);

#endif
