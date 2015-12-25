#ifndef FILE_UTIL_H_
#define FILE_UTIL_H_

#include <sys/types.h>

struct File {
    char* data;
    int   size;
};

void createFifo(const char* fifo_name);

int openFifo(const char* fifo_name);

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
ssize_t readLine(int fd, void* buffer, size_t bufSize);

/**
 * The caller has to provide a constructed File struct.
 * This function creates memory for File.data using malloc.
 * The caller has to free this memory.
 */
int readFileData(const char* filename, struct File* file);


#endif
