#ifndef ERR_UTIL_H_
#define ERR_UTIL_H_

#include <errno.h>
#include <string.h>

void errExit(const char* msg);
void errExitEN(int en, const char* msg);
void errMsg(const char* msg);

#endif
