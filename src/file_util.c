#include "err_util.h"
#include "file_util.h"
#include "log_util.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

void createFifo(const char* fifo_name)
{
    LOG_INFO("Creating named pipe %s for accepting new file names\n", fifo_name);
    umask(0);
    if (mkfifo(fifo_name, S_IRUSR | S_IWUSR) == -1 && errno != EEXIST) {
        errExit("mkfifo\n");
    }
}

int openFifo(const char* fifo_name)
{
    int fifoFd = open(fifo_name, O_RDONLY);
    if (fifoFd == -1) {
        errExit("Open fifo\n");
    }
    // Open extra write descriptor to avoid that we ever see an EOF
    if (open(fifo_name, O_WRONLY) == -1) {
        errExit("Open dummy fifo\n");
    }
    return fifoFd;
}

ssize_t readLine(int fd, void* buffer, size_t bufSize)
{
    memset(buffer, 0, bufSize); // ensure null termination

    size_t totRead = 0;
    ssize_t numRead = 0;
    char ch;
    char *buf = buffer;
    for (;;) {
        numRead = read(fd, &ch,  1);
        if (numRead == -1) {
            if (errno == EINTR)
                continue;
            else
                return -2;
        } else if (numRead == 0) { // EOF
            if (totRead == 0)
                return -1;
            else
                break;
        } else {
            if (ch == '\n')
                break; // do not store the newline character
            if (totRead < bufSize - 1) { // discard all remaining bytes, ensure null termination
                totRead++;
                *buf++ = ch;
            }
        }
    }
    return totRead;
}

int readFileData(const char* filename, struct File* file)
{
    int inputFd;
    struct stat st;

    inputFd = open(filename, O_RDONLY);
    if (inputFd == -1) {
        errMsg("open file\n");
        return -1;
    }

    fstat(inputFd, &st);
    file->size = st.st_size;
    printf("File size: %d.\n", file->size);

    file->data = (char*)malloc(file->size);
    int numRead = read(inputFd, file->data, file->size);
    if (numRead != file->size) {
        fprintf(stderr, "Error while reading file data: read only %d bytes.\n", numRead);
        return -1;
    }
    return 0;
}

