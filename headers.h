#include <stdio.h>      //if you don't use scanf/printf change this include
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>

typedef short bool;
#define true 1
#define false 0

#define SHKEY 300
#define MSGKEY 77


// Enum to define all possible states of a process in the system
enum ProcessState {
    STATE_ARRIVED,
    STATE_STARTED,
    STATE_STOPPED,
    STATE_RESUMED,
    STATE_FINISHED
};

// Process Control Block (PCB) structure to hold all process information
struct PCB {
    int id;                  // Process ID from the input file
    int system_pid;          // Actual PID returned by fork() when the process starts
    int arrival_time;        // The time the process arrived at the scheduler
    int runtime;             // Total execution time required by the process
    int remaining_time;      // Time left for the process to finish execution
    int waiting_time;        // Total time the process spent waiting in the ready queue
    int priority;            // Priority of the process (0 is the highest priority)
    int start_time;          // The time the process started execution
    int finish_time;      // recorded when process signals completion
    int time_executed;    // total CPU time actually consumed
    enum ProcessState state; // Current state of the process
};





struct process
    {
       
        int id;
        int priority;
        int arrival;
        int runtime;
        int remainingTime;
        
    };

    struct message
    {
        long mtype;
        struct process p;
    };

///==============================
//don't mess with this variable//
int * shmaddr;                 //
//===============================



int getClk()
{
    return *shmaddr;
}


/*
 * All process call this function at the beginning to establish communication between them and the clock module.
 * Again, remember that the clock is only emulation!
*/
void initClk()
{
    int shmid = shmget(SHKEY, 4, 0444);
    while ((int)shmid == -1)
    {
        //Make sure that the clock exists
        printf("Wait! The clock not initialized yet!\n");
        sleep(1);
        shmid = shmget(SHKEY, 4, 0444);
    }
    shmaddr = (int *) shmat(shmid, (void *)0, 0);
}


/*
 * All process call this function at the end to release the communication
 * resources between them and the clock module.
 * Again, Remember that the clock is only emulation!
 * Input: terminateAll: a flag to indicate whether that this is the end of simulation.
 *                      It terminates the whole system and releases resources.
*/

void destroyClk(bool terminateAll)
{
    shmdt(shmaddr);
    if (terminateAll)
    {
        killpg(getpgrp(), SIGINT);
    }


    
    
}

