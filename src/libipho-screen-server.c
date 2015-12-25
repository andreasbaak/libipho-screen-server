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
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include "ename.c.inc"
#include "time_util.h"

#define LOG_INFO(...) printf(__VA_ARGS__)

#define DATA_PORT_NUM "1338"
#define HEARTBEAT_PORT_NUM "1339"

#define BACKLOG 0
#define MAX_FN_LENGHT 255;

#define COMMAND_IMAGE_TAKEN 1
#define COMMAND_IMAGE_DATA  2
#define COMMAND_HEARBEAT_PROBE 3;

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
int bindServerSocket(const char* portNum)
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

    if (listen(lfd, BACKLOG) == -1) {
        errExit("Listen\n");
    }

    freeaddrinfo(result);
    return lfd;
}

static pthread_cond_t clientAliveCond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t clientStatusMtx = PTHREAD_MUTEX_INITIALIZER;
typedef enum { DEAD, ALIVE } ClientStatus;
static ClientStatus clientStatus = DEAD;

void wakeDataConnection()
{
    int perr = pthread_cond_signal(&clientAliveCond); // Wake consumer
    if (perr != 0) {
        errExitEN(perr, "pthread_cond_signal");
    }
}

void waitForClientAlive()
{
    int perr = pthread_mutex_lock(&clientStatusMtx);
    if (perr != 0) {
        errExitEN(perr, "pthread_mutex_lock");
    }
    while (clientStatus == DEAD) {
        perr = pthread_cond_wait(&clientAliveCond, &clientStatusMtx);
        if (perr != 0) {
            errExitEN(perr, "pthread_cond_wait");
        }
    }
    perr = pthread_mutex_unlock(&clientStatusMtx);
    if (perr != 0) {
        errExitEN(perr, "pthread_mutex_unlock");
    }
}

void setClientStatus(ClientStatus status)
{
    int perr;
    perr = pthread_mutex_lock(&clientStatusMtx);
    if (perr != 0) {
        errExitEN(perr, "pthread_mutex_unlock");
    }
    clientStatus = status;
    perr = pthread_mutex_unlock(&clientStatusMtx);
    if (perr != 0) {
        errExitEN(perr, "pthread_mutex_unlock");
    }
}

ClientStatus getClientStatus()
{
    int status;
    int perr;
    perr = pthread_mutex_lock(&clientStatusMtx);
    if (perr != 0) {
        errExitEN(perr, "pthread_mutex_unlock");
    }
    status = clientStatus;
    perr = pthread_mutex_unlock(&clientStatusMtx);
    if (perr != 0) {
        errExitEN(perr, "pthread_mutex_unlock");
    }
    return status;
}

/* Return 0 if we cannot write data to the client file descriptor
 * successfully, 1 otherwise.
 */
int isClientAlive(int cfd) {
    char cmd[1];
    cmd[0] = COMMAND_HEARBEAT_PROBE;
    unsigned int i;

    for (i = 0; i < 1; ++i) {
        printf("Sending keepalive probe.\n");
        if (write(cfd, cmd, sizeof(cmd)) != sizeof(cmd)) {
            errMsg("Error on write of keepalive probe");
            return 0;
        }
    }
    return 1;
}

void hearbeat(int cfd)
{
    for (;;) {
        if (!isClientAlive(cfd)) {
            setClientStatus(DEAD);
            break;
        }
        usleep(5e5);
    }
    if (close(cfd) == -1) {
        errMsg("close");
    }
}

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static int commandAvailable = 0;
#define MAX_COMMAND_LENGTH 255
char command[MAX_COMMAND_LENGTH];

// signature is enforced by the pthread_create function
void* readCommandsFromFifo(void* fifo_filename_void) {
    const char* fifo_filename = (const char*) fifo_filename_void;
    char line[MAX_COMMAND_LENGTH];
    int perr;
    int fifoFd = -1;
    int res;

    for (;;) {
        LOG_INFO("Waiting for a command on the FIFO %s\n", fifo_filename);
        if (fifoFd < 0) {
            fifoFd = openImageFilenameFifo(fifo_filename);
        }

        res = readLine(fifoFd, line, sizeof(line));
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

        // We read a line from the fifo. Let's forward it to the consuming thread.
        perr = pthread_mutex_lock(&mtx);
        if (perr != 0) {
            errExitEN(perr, "pthread_mutex_lock");
        }

        memcpy(command, line, MAX_COMMAND_LENGTH);
        commandAvailable = 1;

        perr = pthread_mutex_unlock(&mtx);
        if (perr != 0) {
            errExitEN(perr, "pthread_mutex_unlock");
        }

        perr = pthread_cond_signal(&cond); // Wake consumer
        if (perr != 0) {
            errExitEN(perr, "pthread_cond_signal");
        }
    }
    return NULL;
}

void forwardImages(int cfd)
{
    int perr;
    char commandCopy[MAX_COMMAND_LENGTH];
    char cmd[1];

    for (;;) {
        perr = pthread_mutex_lock(&mtx);
        if (perr != 0)
            errExitEN(perr, "pthread_mutex_lock");

        while (commandAvailable == 0) { // Wait for producer
            struct timespec timeout_time = computeAbsoluteTimeout(2e9);
            perr = pthread_cond_timedwait(&cond, &mtx, &timeout_time);
            if (perr == ETIMEDOUT) {
                // Check wheter the client is still alive.
                if (getClientStatus() == DEAD) {
                    printf("While waiting for commands, the heartbeat signaled that the client is dead.\n");
                    perr = pthread_mutex_unlock(&mtx);
                    // Also close our file descriptor
                    if (close(cfd) == -1) {
                        errMsg("close");
                    }
                    return;
                }
            } else if (perr != 0) { // all other error cases
                errExitEN(perr, "pthread_cond_wait");
            }
        }

        if (commandAvailable == 0) {
            // Nothing is available. Just continue to wait.
            perr = pthread_mutex_unlock(&mtx);
            continue;
        }

        // fetch the command from the other thread
        memcpy(commandCopy, command, MAX_COMMAND_LENGTH);
        commandAvailable = 0;
        perr = pthread_mutex_unlock(&mtx);
        if (perr != 0) {
            errExitEN(perr, "pthread_mutex_unlock");
        }

        if (commandCopy[0] == '+') {
            // We received a special command that indicates
            // the "Image has just been taken" command.
            cmd[0] = COMMAND_IMAGE_TAKEN;
            LOG_INFO("Sending 'Image taken' command.\n");
            if (write(cfd, cmd, sizeof(cmd)) != sizeof(cmd)) {
                fprintf(stderr, "Error on write of command\n");
                break;
            }
            continue;
        }

        LOG_INFO("Trying to read file %s.\n", commandCopy);
        struct File file;
        if (readFileData(commandCopy, &file) == -1) {
            LOG_INFO("Could not read file %s.\n", commandCopy);
            continue;
        }

        char numBytesSplit[4];
        convertInteger(file.size, numBytesSplit);

        LOG_INFO("Sending command 'Image data'\n");

        cmd[0] = COMMAND_IMAGE_DATA;
        if (write(cfd, cmd, sizeof(cmd)) != sizeof(cmd)) {
            fprintf(stderr, "Error on write of command\n");
            free(file.data);
            break;
        }

        LOG_INFO("Sending file size: %d\n", file.size);
        if (write(cfd, numBytesSplit, sizeof(numBytesSplit)) != sizeof(numBytesSplit)) {
            fprintf(stderr, "Error on write of num bytes\n");
            free(file.data);
            break;
        }

        LOG_INFO("Sending file data.\n");
        if (write(cfd, file.data, file.size) != file.size) {
            fprintf(stderr, "Error on writing file data to the socket.\n");
            free(file.data);
            break;
        }

        LOG_INFO("Done sending.\n");
        free(file.data);
    }
    setClientStatus(DEAD);
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
void acceptDataConnection()
{
    struct sockaddr_storage claddr;
    int cfd;
    socklen_t addrlen;

    for (;;) { // Serve only one client connection at a time.
        LOG_INFO("Waiting for the client heartbeat.\n");
        waitForClientAlive();

        int lfd = bindServerSocket(DATA_PORT_NUM);
        addrlen = sizeof(struct sockaddr_storage);
        LOG_INFO("Waiting for an image receiver to connect.\n");
        cfd = accept(lfd, (struct sockaddr*) &claddr, &addrlen);
        if (cfd == -1) {
            errMsg("accept");
            continue;
        }
        LOG_INFO("Connection accepted.\n");

        forwardImages(cfd);
        if (close(cfd) == -1) {
          errMsg("close");
        }
        if (close(lfd) == -1) {
          errMsg("close");
        }
    }
}


void* acceptHeartbeatConnection()
{
    struct sockaddr_storage claddr;
    int cfd;
    socklen_t addrlen;

    for (;;) { // Serve only one client connection at a time.
        addrlen = sizeof(struct sockaddr_storage);
        int lfd = bindServerSocket(HEARTBEAT_PORT_NUM);
        LOG_INFO("Waiting for an client to connect to the heartbeat channel.\n");
        cfd = accept(lfd, (struct sockaddr*) &claddr, &addrlen);
        if (cfd == -1) {
            errMsg("accept hearbeat");
            continue;
        }
        LOG_INFO("Hearbeat connection accepted.\n");
        setClientStatus(ALIVE);
        wakeDataConnection();
        hearbeat(cfd);
        // hearbeat will only return if the cfd is close.
        if (close(lfd) != 0) {
            errExit("hearbeat close_lfd");
        }
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

    // Ignore the sigpipe so that we can find out about a broken connection
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        errExit("signal\n");

    createImageFilenameFifo(fifo_filename);

    // Create a thread that reads commands from the pipe
    // and forwards the commands to our main thread.
    pthread_t command_tid;
    if (pthread_create(&command_tid, NULL, readCommandsFromFifo, (void*)fifo_filename) != 0) {
        fprintf(stderr, "Error while trying to create a thread.\n");
        exit(1);
    }

    // Create a thread that sends a heartbeat to the client
    // in order to check whether she is alive
    pthread_t heartbeat_tid;
    if (pthread_create(&heartbeat_tid, NULL, acceptHeartbeatConnection, NULL) != 0) {
        fprintf(stderr, "Error while trying to create a thread.\n");
        exit(1);
    }

    acceptDataConnection();
    return 0;
}

