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

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ename.c.inc"

#define LOG_INFO(...) printf(__VA_ARGS__)

#define PORT_NUM "1338"
#define BACKLOG 1
#define MAX_FN_LENGHT 255;

#define COMMAND_IMAGE_TAKEN 1
#define COMMAND_IMAGE_DATA  2

void errExit(const char* msg)
{
    fprintf(stderr, "[%s %s] %s\n", (errno > 0) ? ename[errno] : "?UNKNOWN?", strerror(errno), msg);
    exit(1);
}

void errMsg(const char* msg)
{
    int savedErrno;
    savedErrno = errno;
    fprintf(stderr, "[%s %s] %s\n", (errno > 0) ? ename[errno] : "?UNKNOWN?", strerror(errno), msg);
    errno = savedErrno;
}


/**
 * numBytesSplit has to point to at least 4 bytes of memory.
 */
void convertInteger(int fileSize, char* numBytesSplit)
{
    int i;
    for (i = 0; i < 4; ++i) {
        numBytesSplit[i] = (char) ((fileSize % 0xff) & 0xff);
        fileSize /= 0xff;
    }
}

void createImageFilenameFifo(const char* fifo_name)
{
    LOG_INFO("Creating named pipe %s for accepting new file names\n", fifo_name);
    umask(0);
    if (mkfifo(fifo_name, S_IRUSR | S_IWUSR) == -1
            && errno != EEXIST)
        errExit("mkfifo\n");
}

int openImageFilenameFifo(const char* fifo_name)
{
    int fifoFd = open(fifo_name, O_RDONLY);
    if (fifoFd == -1)
        errExit("Open fifo\n");
    // Open extra write descriptor to avoid that we ever see an EOF
    if (open(fifo_name, O_WRONLY) == -1)
        errExit("Open dummy fifo\n");
    // ignore the sigpipe just in case
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        errExit("signal\n");
    return fifoFd;
}


/**
  * Read data from fd and store it into the buffer
  * until a newline character is found.
  * This function ensures that the buffer is null terminated.
  * The number of read characters is returned.
  * The newline character is not stored into buffer and not counted.
  *
  * \return -2 if an unknown error occurred while reading from the fifo.
  *         -1 if EOF is encountered and we have not read any data
  *          0 if we have read only a newline character
  *         >0 if we have received a valid string, return its length.
  */
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

struct File {
    char* data;
    int   size;
};

/**
 * The caller has to provide a constructed File struct.
 * This function creates memory for File.data using malloc.
 * The caller has to free this memory.
 */
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


/**
 *
 * \return file descriptor corresponding to the server socket
 */
int bindServerSocker()
{
    LOG_INFO("Binding server socket.\n");

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
    if (getaddrinfo(NULL, PORT_NUM, &hints, &result) != 0)
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

    if (listen(lfd, BACKLOG) == -1) {
        errExit("Listen\n");
    }

    freeaddrinfo(result);
    return lfd;
}

void forwardImages(const char* fifo_filename, int cfd)
{
    char filename[255];
    int res;
    int fifoFd = -1;
    for (;;) {
        LOG_INFO("Waiting for an image filename on the FIFO %s\n", fifo_filename);
        if (fifoFd < 0) {
            fifoFd = openImageFilenameFifo(fifo_filename);
        }

        res = readLine(fifoFd, filename, sizeof(filename));
        if (res == -1) {
            fprintf(stderr, "FIFO was closed.\n");
            fifoFd = -1;
            continue;
        }
        if (res == -2) {
            fprintf(stderr, "Error while reading from FIFO. Trying again...\n");
            continue;
        }
        if (res == 0) {
            fprintf(stderr, "Received EOF on the FIFO. Trying again...\n");
            fifoFd = -1;
            continue;
        }

        if (res == 1 && filename[0] == '+') {
            // We received a special command that indicates
            // the "Image has just been taken" command.
            char command[1];
            command[0] = COMMAND_IMAGE_TAKEN;
            LOG_INFO("Sending 'Image taken' command.\n");
            if (write(cfd, command, sizeof(command)) != sizeof(command)) {
                fprintf(stderr, "Error on write of command\n");
                break;
            }
            continue; // wait for the next image filename
        }

        LOG_INFO("Trying to read file %s.\n", filename);
        struct File file;
        if (readFileData(filename, &file) == -1) {
            LOG_INFO("Could not read file %s.\n", filename);
            continue;
        }

        char numBytesSplit[4];
        convertInteger(file.size, numBytesSplit);

        LOG_INFO("Sending command 'Image data'\n");
        char command[1];
        command[0] = COMMAND_IMAGE_DATA;
        if (write(cfd, command, sizeof(command)) != sizeof(command)) {
            fprintf(stderr, "Error on write of command\n");
            break;
        }

        LOG_INFO("Sending file size: %d\n", file.size);
        if (write(cfd, numBytesSplit, sizeof(numBytesSplit)) != sizeof(numBytesSplit)) {
            fprintf(stderr, "Error on write of num bytes\n");
            break;
        }

        LOG_INFO("Sending file data.\n");
        if (write(cfd, file.data, file.size) != file.size) {
            fprintf(stderr, "Error on writing file data to the socket.\n");
            break;
        }

        LOG_INFO("Done sending.\n");
        free(file.data);
    }
    if (close(cfd) == -1) {
        errMsg("close");
    }
}

/**
 * Wait an incoming client connection.
 * As soon as a client connect, forward commands and
 * image filenames taken from the fifo to the client.
 * If any error occurrs while writing to the client,
 * the connection is closed and we wait for the
 * next incoming client.
 */
void acceptClientConnection(int lfd, const char* fifo_filename)
{
    struct sockaddr_storage claddr;
    int cfd;
    socklen_t addrlen;

    for (;;) { // Serve only one client connection at a time.
        addrlen = sizeof(struct sockaddr_storage);
        LOG_INFO("Waiting for an image receiver to connect.\n");
        cfd = accept(lfd, (struct sockaddr*) &claddr, &addrlen);
        if (cfd == -1) {
            errMsg("accept");
            continue;
        }
        LOG_INFO("Connection accepted.\n");
        forwardImages(fifo_filename, cfd);
    }
}

void usage(const char* programName)
{
    printf("Send image data to the screen of the libipho photobooth.\n");
    printf("\n");
    printf("Usage: %s fifo_filename\n", programName);
    printf("\n");
    printf("  fifo_filename: the file name of the fifo under which\n");
    printf("                 this server receives commands.\n");
    exit(1);
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        usage(argv[0]);
    }
    const char* fifo_filename = argv[1];
    createImageFilenameFifo(fifo_filename);
    int lfd = bindServerSocker();
    acceptClientConnection(lfd, fifo_filename);
    return 0;
}
