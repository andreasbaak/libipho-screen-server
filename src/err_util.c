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

