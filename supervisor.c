/**
 * @file supervisor.c
 * @author Gurbinder Singh
 * @date 2018-06-11
 *
 * @brief   This program reads and evalutes the solutions calculated by the
 *          the generator and saves the best solutions.
 */
#include "arcset.h"


struct SharedMemory *shared_space = NULL;
const char *progName;
sem_t *freeSpace_sem = NULL;
sem_t *usedSpace_sem = NULL;
sem_t *mutex_sem = NULL;
unsigned int currentBest = INT_MAX;
int bestSol[MAX_SOL_LEN] = {0};
int shm_fd = -1;


/**
 * @brief   Wraps up the execution when the program terminates.
 *          Also used as signal handler.
 * @param sig   Signal passeb by signal handler.
*/
static void finishUp(int sig);

/**
 * @brief   Read the solution written to the shared memory area.
*/
static void readSolution(void);



/**
 * @brief   Sets up the resources and initates the reading of shared memory.
 * @param argc  Number of arguments supplied to the program.
 * @param argv  The arguments supplied to the program.
 * @return  Returns EXIT_SUCCESS if the program terminates successfully.
*/
int main(int argc, char **argv)
{
    if(argc > 1)
    {
        bailOut(1, "This program takes no arguments!");
    }
    else
    {
        progName = argv[0];
    }
    struct sigaction sa;
    sa.sa_handler = finishUp;
    sa.sa_flags = 0;

    if (sigfillset(&sa.sa_mask) < 0)
    {
       bailOut(EXIT_FAILURE, "Failed to set up signal handler");
    }
    if (sigaction(SIGINT, &sa, NULL) < 0 || sigaction(SIGTERM, &sa, NULL))
    {
        bailOut(EXIT_FAILURE, "Failed to set up signal handler");
    }


    shm_fd = shm_open(SHM_NAME, O_CREAT | O_EXCL | O_RDWR, PERMISSION);
    if(-1 == shm_fd)
    {
        bailOut(1, "Failed to create or open shared memory");
    }

    if(-1 == ftruncate(shm_fd, sizeof(struct SharedMemory)))
    {
        bailOut(1, "Failed to truncate shared memory");
    }

    shared_space = mmap(NULL, sizeof(struct SharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if(MAP_FAILED == shared_space)
    {
        bailOut(1, "Memory mapping failed");
    }

    freeSpace_sem = sem_open(SEM_FREE_SPACE, O_CREAT | O_EXCL, PERMISSION, MAX_SOLS);
    usedSpace_sem = sem_open(SEM_USED_SPACE, O_CREAT | O_EXCL, PERMISSION, 0);
    mutex_sem = sem_open(SEM_MUTEX, O_CREAT | O_EXCL, PERMISSION, 1);
    if(SEM_FAILED == freeSpace_sem || SEM_FAILED == usedSpace_sem || SEM_FAILED == mutex_sem)
    {
        bailOut(1, "Could not create all necessary semaphores");
    }
    shared_space->write_pos = 0;
    shared_space->read_pos = 0;
    shared_space->quit = 0;
    for(int i= 0; i< MAX_SOLS; i++)
    {
        shared_space->solutionLength[i] = INT_MAX;
    }
    
    for(int i=0; i < MAX_SOLS; i++)
    {
        for(int j=0; j < MAX_SOL_LEN; j++)
        {
            shared_space->data[i][j] = -1;
        }
    }

    for(int i=0; i < MAX_SOL_LEN; i++)
    {
        bestSol[i] = -1;
    }
    while(currentBest > 0)
    {
        readSolution();
    }
    if(currentBest < 1)
    {
        printf("[%s] The graph is acyclic!\n", progName);
    }
   finishUp(0);
   return EXIT_SUCCESS;
}




static void readSolution(void)
{
    sem_wait(usedSpace_sem);
    int readPos = shared_space->read_pos;
    int tempBest = currentBest;
    
    if(shared_space->solutionLength[readPos] < currentBest)
    {
        for(int i = 0; i < MAX_SOL_LEN; i+=2)
        {
            bestSol[i] = shared_space->data[readPos][i];
            bestSol[i+1] = shared_space->data[readPos][i+1];
        }
        currentBest = shared_space->solutionLength[readPos];
    }
    sem_post(freeSpace_sem);
    shared_space->read_pos = (readPos + 1) % MAX_SOLS;
    
    if(currentBest < tempBest)
    {
        printf("[%s] Solution with %d edges: ", progName, currentBest);
        for(int i = 0; bestSol[i] != -1 && i < MAX_SOL_LEN; i+=2)
        {
            printf("%d-%d ", bestSol[i], bestSol[i+1]);
        }
        printf("\n");
    }
}




void bailOut(int exitCode, char *msg)
{
    if (msg != NULL)
    {
        fprintf(stderr, "%s: ", progName);
        fprintf(stderr, "%s\n", msg);
    }
    if (errno != 0)
    {
        fprintf(stderr, "%s\n", strerror(errno));
    }
    cleanup();
    exit(exitCode);
}




static void finishUp(int sig)
{
    shared_space->quit = QUIT;
    cleanup();
    exit(EXIT_SUCCESS);
}




void cleanup(void)
{
    if(-1 == munmap(shared_space, sizeof(struct SharedMemory)))
    {
        fprintf(stderr, "%s: Failed to delete mapping\n", progName);
        if (errno != 0)
        {
            fprintf(stderr, "%s\n", strerror(errno));
        }
    }
    if(-1 == close(shm_fd))
    {
        fprintf(stderr, "%s: Failed to close shared memory file descriptor\n", progName);
        if (errno != 0)
        {
            fprintf(stderr, "%s\n", strerror(errno));
        }
    }
    if(-1 == shm_unlink(SHM_NAME))
    {
        fprintf(stderr, "%s: Could not unlink shared memory\n", progName);
        if (errno != 0)
        {
            fprintf(stderr, "%s\n", strerror(errno));
        }
    }
    if(-1 == sem_close(freeSpace_sem) || -1 == sem_close(usedSpace_sem) || -1 == sem_close(mutex_sem))
    {
        fprintf(stderr, "%s: Could not close all semaphores\n", progName);
        if (errno != 0)
        {
            fprintf(stderr, "%s\n", strerror(errno));
        }
    }
    if((-1 == sem_unlink(SEM_MUTEX)) || (-1 == sem_unlink(SEM_FREE_SPACE)) || (-1 == sem_unlink(SEM_USED_SPACE)))
    {
        fprintf(stderr, "%s: Could not unlink all semaphores\n", progName);
        if (errno != 0)
        {
            fprintf(stderr, "%s\n", strerror(errno));
        }
    }
}