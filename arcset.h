/**
 * @file arcset.h
 * @author Gurbinder Singh
 * @date 2018-06-11
 *
 * @brief   This file contains all the declarations needed by
 *          both the supervisor and generator programs.
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <limits.h>
#include <string.h>

#define SHM_NAME "/1526071_sharedMemory"
#define SEM_MUTEX "/1526071_mutexSemaphore"
#define SEM_FREE_SPACE "/1526071_freeSpaceSemaphore"
#define SEM_USED_SPACE "/1526071_usedSpaceSemaphore"

#define MAX_SOLS 10
#define MAX_SOL_LEN 100
    /* the above constant has to be twice the size of the allowed solution 
       length, since it'll store nodes not entire edges! */
#define PERMISSION 0600 
#define QUIT 1
#define RUN 0


struct SharedMemory
{
    int data[MAX_SOLS][MAX_SOL_LEN];
    unsigned int solutionLength[MAX_SOLS];
    int write_pos;
    int read_pos;
    sig_atomic_t quit;
};


/**
 * @brief   If the message is not NULL or a errno is set, both are 
 *          printed to the console and after cleanup the program
 *          is terminated with the supplied exit code.
*/
void bailOut(int, char*);

/**
 * @brief   Performs cleanup of allocated resources.
*/
void cleanup(void);