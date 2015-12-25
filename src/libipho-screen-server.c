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

#include "boolean_util.h"
#include "err_util.h"
#include "file_util.h"
#include "log_util.h"
#include "net_util.h"
#include "time_util.h"

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

#define DATA_PORT_NUM "1338"
#define HEARTBEAT_PORT_NUM "1339"

#define BACKLOG 0
#define MAX_FN_LENGHT 255;

#define COMMAND_IMAGE_TAKEN 1
#define COMMAND_IMAGE_DATA  2
#define COMMAND_HEARBEAT_PROBE 3;

typedef enum { DEAD, ALIVE } ClientStatus;
// clientStatus indicates wheter the Android app is connected or not.
// Access to the clientStatus is secured via the mutex ClientStatusMtx.
// A thread can be waiting for the clientStatus to become ALIVE.
// To this end, we set up the clientAliveCond conditional variable
// that is notified once the client has connected.
static ClientStatus clientStatus = DEAD;
static pthread_mutex_t clientStatusMtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t clientAliveCond = PTHREAD_COND_INITIALIZER;

/**
 * Wake up threads that wait for the client app to become available.
 */
void wakeClientAliveWaiter()
{
    int perr = pthread_cond_signal(&clientAliveCond); // Wake waiting thread
    if (perr != 0) {
        errExitEN(perr, "pthread_cond_signal");
    }
}

/**
 * Suspend the current thread until a client app has become available.
 * Returns immediately if the client is already available.
 */
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

/**
 * Safely set the client status secured by the corresponding mutex.
 */
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

/**
 * Safely get the client status secured by the corresponding mutex.
 */
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

/**
 * Writes a heartbeat probe to the provided file descriptor.
 * If an error occurrs while writing, assume that the client is not alive.
 *
 * \param cfd
 * File descriptor that we send a heartbeat probe to.
 * \return
 * TRUE if the client is alive, FALSE otherwise.
 *
 */
Boolean isClientHearbeatAlive(int cfd) {
    char cmd[1];
    cmd[0] = COMMAND_HEARBEAT_PROBE;

    if (!writeFully(cfd, cmd, sizeof(cmd))) {
        errMsg("Error on write of keepalive probe");
        return FALSE;
    }
    return TRUE;
}


/**
 * Continuously sends a hearbeat probe to the client.
 * The function returns as soon the heartbeat channel
 * cannot be written to anymore.
 *
 * \param cfd
 * File descriptor to send the probe to.
 */
void hearbeat(int cfd)
{
    for (;;) {
        if (!isClientHearbeatAlive(cfd)) {
            setClientStatus(DEAD);
            break;
        }
        usleep(5e5);
    }
    if (close(cfd) == -1) {
        errMsg("close");
    }
}


static pthread_mutex_t commandMtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t commandCond = PTHREAD_COND_INITIALIZER;
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
            fifoFd = openFifo(fifo_filename);
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
        perr = pthread_mutex_lock(&commandMtx);
        if (perr != 0) {
            errExitEN(perr, "pthread_mutex_lock");
        }

        memcpy(command, line, MAX_COMMAND_LENGTH);
        commandAvailable = 1;

        perr = pthread_mutex_unlock(&commandMtx);
        if (perr != 0) {
            errExitEN(perr, "pthread_mutex_unlock");
        }

        perr = pthread_cond_signal(&commandCond); // Wake consumer
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
        perr = pthread_mutex_lock(&commandMtx);
        if (perr != 0)
            errExitEN(perr, "pthread_mutex_lock");

        while (commandAvailable == 0) { // Wait for producer
            struct timespec timeout_time = computeAbsoluteTimeout(5e8);
            perr = pthread_cond_timedwait(&commandCond, &commandMtx, &timeout_time);
            if (perr == ETIMEDOUT) {
                // Check wheter the client is still alive.
                if (getClientStatus() == DEAD) {
                    printf("While waiting for commands, the heartbeat signaled that the client is dead.\n");
                    perr = pthread_mutex_unlock(&commandMtx);
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
            perr = pthread_mutex_unlock(&commandMtx);
            continue;
        }

        // fetch the command from the other thread
        memcpy(commandCopy, command, MAX_COMMAND_LENGTH);
        commandAvailable = 0;
        perr = pthread_mutex_unlock(&commandMtx);
        if (perr != 0) {
            errExitEN(perr, "pthread_mutex_unlock");
        }

        if (commandCopy[0] == '+') {
            // We received a special command that indicates
            // the "Image has just been taken" command.
            cmd[0] = COMMAND_IMAGE_TAKEN;
            LOG_INFO("Sending 'Image taken' command.\n");
            if (!writeFully(cfd, cmd, sizeof(cmd))) {
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

        LOG_INFO("Transmitting file %s.\n", commandCopy);

        char numBytesSplit[4];
        intToByteArray(file.size, numBytesSplit);

        cmd[0] = COMMAND_IMAGE_DATA;
        if (!writeFully(cfd, cmd, sizeof(cmd))) {
            fprintf(stderr, "Error on write of command\n");
            free(file.data);
            break;
        }
        if (!writeFully(cfd, numBytesSplit, sizeof(numBytesSplit))) {
            fprintf(stderr, "Error on write of num bytes\n");
            free(file.data);
            break;
        }
        if (!writeFully(cfd, file.data, file.size)) {
            fprintf(stderr, "Error on writing file data to the socket.\n");
            free(file.data);
            break;
        }

        LOG_INFO("File has been transmitted.\n");
        free(file.data);
    }
    setClientStatus(DEAD);
    if (close(cfd) == -1) {
        errMsg("close");
    }
}

/**
 * Wait for an incoming client connection.
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

        int lfd = bindServerSocket(DATA_PORT_NUM, BACKLOG);
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
        int lfd = bindServerSocket(HEARTBEAT_PORT_NUM, BACKLOG);
        LOG_INFO("Waiting for a client to connect to the heartbeat channel.\n");
        cfd = accept(lfd, (struct sockaddr*) &claddr, &addrlen);
        if (cfd == -1) {
            errMsg("accept heartbeat");
            continue;
        }
        LOG_INFO("Hearbeat connection accepted.\n");
        setClientStatus(ALIVE);
        wakeClientAliveWaiter();
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

    createFifo(fifo_filename);

    // Create a thread that reads commands from the pipe
    // and forwards the commands to our main thread.
    pthread_t command_tid;
    int perr;
    perr = pthread_create(&command_tid, NULL, readCommandsFromFifo, (void*)fifo_filename);
    if (perr != 0) {
        errExitEN(perr, "Error while trying to create a thread.");
    }

    // Create a thread that sends a heartbeat to the client
    // in order to check whether she is alive
    pthread_t heartbeat_tid;
    perr = pthread_create(&heartbeat_tid, NULL, acceptHeartbeatConnection, NULL);
    if (perr != 0) {
        errExitEN(perr, "Error while trying to create a thread.");
    }

    acceptDataConnection();
    return 0;
}

