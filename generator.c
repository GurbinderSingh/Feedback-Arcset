/**
 * @file generator.c
 * @author Gurbinder Singh
 * @date 2018-06-11
 *
 * @brief   This program reads the edges supplied to it at execution time
 *          at calculates the feedback arcs based on them.
 */
#include "arcset.h"
#include <time.h>

#define BASE 10

struct Edge
{
    int fromNode;
    int toNode;
    struct Edge *next;
};


const char* progName;
struct Edge *edgeList = NULL;
struct SharedMemory *shared_space = NULL;

int *nodeSet = NULL;
int sizeofNodeSet = 0;
int arcSet[MAX_SOL_LEN] = {0};
int sizeArcSet = 0;
int shm_fd = -1;
sem_t *freeSpace_sem = NULL;
sem_t *usedSpace_sem = NULL;
sem_t *mutex_sem = NULL;


/**
 * @brief   Prints the usage message to the console.
*/
static void usage(void);

/**
 * @brief   Prints the current saved feedback arc set to the console.
*/
static void printArcSet(void);

/**
 * @brief   Performs the Fisher Yates shuffle algorithm on the node set.
*/
static void shuffleNodes(void);

/**
 * @brief   Prints the current node set to the console.
*/
//static void printNodeSet(void);

/**
 * @brief   Since the size of the node set is always incremented by a fixed
 *          number, this function truncates off the unnecessary part of the
 *          array containing the nodes.
*/
static void adjustSetSize(void);

/**
 * @brief   Calculates a feedback arc for the current node set.
*/
static void findFeedbackArc(void);

/**
 * @brief   Reads and checks the arguments given to the program.
 * @param argc   Is the number of arguments.
 * @param argv    Are the arguments.
*/
static void getArgs(int argc, char **argv);

/**
 * @brief   Takes a string as input and extracts the nodes from it.
 * @param int* 
*/
static void getNodes(int* fromNode, int* toNode, char* edge);

/**
 * @brief Adds a node to the current set of nodes.
 * @param node  Node to be added.
*/
static void addToSet(int node);

/**
 * @brief Checks if there is edge from node1 to node2.
 * @param node1
 * @param node2
 * @return  If the to nodes are connected 'y' is return, otherwise 'n'
*/
static char areConnected(int node1, int node2);

/**
 * @brief   Writes the solution to the shared memory area.
*/
static void writeSolution(void);



/**
 * @brief   Sets up the necessary resources and initates the calculation and 
 *          writing of the feedback arc and writing it to the shared memory.
 * @param argc  Number of arguments supplied to the program.
 * @param argv  Arguments supplied to the program.
 * @return  Returns EXIT_SUCCESS if the program executes normally.
*/
int main(int argc, char **argv)
{
    getArgs(argc, argv);
    for(int i = 0; i < MAX_SOL_LEN; i++)
    {
        arcSet[i] = -1;
    }
    srand(time(NULL));

    shm_fd = shm_open(SHM_NAME, O_RDWR, PERMISSION);
    if(-1 == shm_fd)
    {
        bailOut(1, "Failed to create or open shared memory");
    }
    shared_space = mmap(NULL, sizeof(struct SharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if(MAP_FAILED == shared_space)
    {
        bailOut(1, "Memory mapping failed");
    }

    freeSpace_sem = sem_open(SEM_FREE_SPACE, 0);
	usedSpace_sem = sem_open(SEM_USED_SPACE, 0);
    mutex_sem = sem_open(SEM_MUTEX, 0);
    if(SEM_FAILED == freeSpace_sem || SEM_FAILED == usedSpace_sem || SEM_FAILED == mutex_sem)
    {
        bailOut(1, "Could not open all necessary semaphores");
    }

    
    while(RUN == shared_space->quit)
    {
        findFeedbackArc();
        writeSolution();
    }
    cleanup();
    return EXIT_SUCCESS;
}




static void writeSolution(void)
{
    if(RUN != shared_space->quit)
    {
        bailOut(0, NULL);
    }
    if(sizeArcSet > MAX_SOL_LEN/2)
    {
        return;
    }
    //printNodeSet();
    printArcSet();
    

    sem_wait(mutex_sem);
    sem_wait(freeSpace_sem);

    int writePos = shared_space->write_pos;
    for(int i = 0; i < MAX_SOL_LEN; i+=2)
    {
        shared_space->data[writePos][i] = arcSet[i];
        shared_space->data[writePos][i+1] = arcSet[i+1];
    }
    shared_space->solutionLength[writePos] = sizeArcSet;
    sem_post(usedSpace_sem);
    sem_post(mutex_sem);
    shared_space->write_pos = (writePos + 1) % MAX_SOLS;
}




static void findFeedbackArc(void)
{
    sizeArcSet = 0;
    int solCounter = 0;
    
    
    shuffleNodes();
    for(int i = sizeofNodeSet-1; i > 0; i--)
    {
        for(int j = i; j >=0; j--)
        {
            if('y' == areConnected(nodeSet[i], nodeSet[j]))
            {
                sizeArcSet++;
                arcSet[solCounter] = nodeSet[i];
                arcSet[solCounter+1] = nodeSet[j];
                solCounter+=2;
            }
        }
    }
    while(solCounter < MAX_SOL_LEN)
    {
        arcSet[solCounter] = -1;
        solCounter++;
    }
}




static void shuffleNodes(void)
{
    for(int i = sizeofNodeSet-1; i > 0; i--)
    {
        int randomIndex = (int) rand() % i;
        int randomNode = nodeSet[randomIndex];
        int currentNode = nodeSet[i];
        
        nodeSet[randomIndex] = currentNode;
        nodeSet[i] = randomNode;
    }
}



static char areConnected(int node1, int node2)
{
    struct Edge *currElem = edgeList;
    char connected = 'n';

    while(currElem != NULL)
    {
        if(currElem->fromNode == node1 && currElem->toNode == node2)
        {
            connected = 'y';
            break;
        }
        currElem = currElem->next;
    }
    return connected;
}



/*
static void printNodeSet(void)
{
    printf("[ ");
    for(int i = 0; i < sizeofNodeSet; i++) 
    {
        printf("%d", nodeSet[i]);
        if(i+1 < sizeofNodeSet)
        {
            printf(", ");
        }
    }
    printf(" ] -> ");
}*/


static void printArcSet(void)
{
    printf("%d edges: { ", sizeArcSet);
    for(int i=0; arcSet[i] != -1 && i < MAX_SOL_LEN; i += 2)
    {
        printf("%d-%d ", arcSet[i], arcSet[i+1]);
    }
    printf("}\n");
}




static void getArgs(int argc, char **argv)
{
    progName = argv[0];
    /*
        If no arguments were given, terminate the program.
    */
    if(argc <= 1)
    {
        usage();
    }
    
    edgeList = malloc(sizeof(struct Edge));
    nodeSet = calloc(sizeofNodeSet = 20, sizeof(int));
    if(edgeList == NULL || nodeSet == NULL)
    {
        bailOut(1, "Could not allocate memory.");
    }
    struct Edge *currentListElem = edgeList;
    for(int i = 0; i < sizeofNodeSet;  i++)
    {
        nodeSet[i] = -1;
    }
    /*
        Go through all arguments, get the edges and nodes and
        allocate the next list element and make temp point to 
        it (the head is still stored in edgeList).
    */
    for(int i = 1; i < argc; i++)
    {
        int fromNode = -1;
        int toNode = -1;
        getNodes(&fromNode, &toNode, argv[i]);

        currentListElem->fromNode = fromNode;
        currentListElem->toNode = toNode;
        addToSet(fromNode);
        addToSet(toNode);
        
        if(i+1 < argc)
        {
            currentListElem->next = malloc(sizeof(struct Edge));
            if(currentListElem == NULL)
            {
                bailOut(1, "Could not allocate memory.");
            }
        }
        currentListElem = currentListElem->next;
    }
    adjustSetSize();
}



/**
 * @brief If the array containing the nodes is larger then the number of nodes 
 *        inside it, then the unnecessary part is truncated off, to make the 
 *        shuffling easier.
 */
static void adjustSetSize(void)
{
    int i;
    for(i = sizeofNodeSet-1; nodeSet[i] == -1 && i >= 0; i--);
    
    if(i+1 < sizeofNodeSet)
    {
        nodeSet= realloc(nodeSet, (sizeofNodeSet= i+1)*sizeof(int));
        if(nodeSet == NULL)
        {
            bailOut(1, "Memory allocation failed.");
        }
    }
}




static void getNodes(int *fromNode, int *toNode, char *edge)
{
    char *endptr;
    *fromNode = strtol(edge, &endptr, BASE);
    /*
        Since strtol() stores the the address of the first invalid character in 
        endptr => if endptr and edge are the same, it means the entire string is 
        invalid. The same applies, if only one node was supplied.
    */
    if(0 == strcmp(edge, endptr) || 0 == strcmp(endptr+1, ""))
    {            
        usage();
    }
    *toNode = strtol(endptr+1, &endptr, BASE);
    /*
        If either node is negative or there is still something after the second
        node, then the input was invalid.
    */
    if(*fromNode < 0 || *toNode < 0 || 0 != strcmp(endptr, ""))
    {
        usage();
    }
    if(errno == ERANGE)
    {
        bailOut(1, "Check node values.");
    }
}




static void addToSet(int node)
{
    char nodeExists = 'n';
    int i;
    

    for(i = 0; nodeSet[i] != -1 && i < sizeofNodeSet; i++)
    {
        if(node == nodeSet[i])
        {
            nodeExists = 'y';
        }
    }
    /*
        If the array is completely filled, increase it's size and mark all 
        new positions as empty.
    */
    if(i+1 >= sizeofNodeSet)
    {
        int prevSize = sizeofNodeSet;
        
        nodeSet = realloc(nodeSet, (sizeofNodeSet += 20)*sizeof(int));
        if(nodeSet == NULL)
        {
            bailOut(1, "Memory allocation failed.");
        }
        for(int j = prevSize; j < sizeofNodeSet; j++)
        {
            nodeSet[j] = -1;
        }
    }
    if(nodeExists == 'n')
    {
        nodeSet[i] = node;
    } 
}




static void usage(void)
{
    fprintf(stderr, "Usage: %s egde1 egdge2 ...\n", progName);
    fprintf(stderr, "Example: %s 1-2 2-3 3-4 ...\n", progName);
    cleanup();
    exit(1);
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




void cleanup(void)
{
    if(NULL != shared_space && -1 == munmap(shared_space, sizeof(struct SharedMemory)))
    {
        fprintf(stderr, "%s: Failed to delete mapping\n", progName);
        if (errno != 0)
        {
            fprintf(stderr, "%s\n", strerror(errno));
        }
    }
    if(shm_fd >= 0 && -1 == close(shm_fd))
    {
        fprintf(stderr, "%s: Failed to close shared memory file descriptor\n", progName);
        if (errno != 0)
        {
            fprintf(stderr, "%s\n", strerror(errno));
        }
    }
    if(NULL != freeSpace_sem && -1 == sem_close(freeSpace_sem))
    {
        fprintf(stderr, "%s: Could not close semaphore for free space\n", progName);
        if (errno != 0)
        {
            fprintf(stderr, "%s\n", strerror(errno));
        }
    }
    if(NULL != usedSpace_sem && -1 == sem_close(usedSpace_sem))
    {
        fprintf(stderr, "%s: Could not close semaphore for used space\n", progName);
        if (errno != 0)
        {
            fprintf(stderr, "%s\n", strerror(errno));
        }
    }
    if(NULL != mutex_sem && -1 == sem_close(mutex_sem))
    {
        fprintf(stderr, "%s: Could not close semaphore for mutual exclusion\n", progName);
        if (errno != 0)
        {
            fprintf(stderr, "%s\n", strerror(errno));
        }
    }
    while(NULL != edgeList)
    {
        struct Edge *temp = edgeList;
        edgeList = edgeList->next;

        free(temp);
    }

    free(nodeSet);
}
