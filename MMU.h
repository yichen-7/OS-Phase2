#ifndef MMU_H
#define MMU_H

#include <stdio.h>
#include <stdlib.h>

typedef short bool;
#define true 1
#define false 0

// Core memory specs
#define RAM_SIZE 512
#define PAGE_SIZE 16
#define NUM_FRAMES (RAM_SIZE / PAGE_SIZE) 

// Enum to define all possible states of a process in the system
// Moved here from headers.h so MMU.c can use it without including headers.h
enum ProcessState {
    STATE_ARRIVED,
    STATE_STARTED,
    STATE_STOPPED,
    STATE_RESUMED,
    STATE_FINISHED
};

// Process Control Block (PCB) structure to hold all process information
// Moved here from headers.h so MMU.c can use it without including headers.h
struct PCB {
    int id;                  // Process ID from the input file
    int system_pid;          // Actual PID returned by fork() when the process starts
    int arrival_time;        // The time the process arrived at the scheduler
    int runtime;             // Total execution time required by the process
    int remaining_time;      // Time left for the process to finish execution
    int waiting_time;        // Total time the process spent waiting in the ready queue
    int priority;            // Priority of the process (0 is the highest priority)
    int start_time;          // The time the process started execution
    int finish_time;         // recorded when process signals completion
    int time_executed;       // total CPU time actually consumed
    int page_table_frame;    // The frame number where the process's page table is stored
    int base;
    int limit;
    enum ProcessState state; // Current state of the process
};

// Structs shared across the team
typedef struct {
    int valid;         
    int frame_number;  
    int R;             
    int M;             
} PageTableEntry; // Represents an entry in a page table : valid bit, frame number, R and M bits 

typedef struct {
    int is_free;       // 1 if available
    int pid;           // Owner process ID
    int virtual_page;  // Which virtual page is loaded here
    int is_page_table; // 1 = page table frame, NEVER evict
    int R;             // Referenced bit
    int M;             // Modified bit
} FrameEntry; // Represents an entry in the frame table : free/occupied, owner PID, virtual page loaded, is it a page table frame, R and M bits

// These are defined in scheduler.c (via headers.h) — declared here so MMU.c can use them
extern int *ram_shmaddr;
extern int getClk();

// Function Prototypes
void initMemoryLog();
void initializeFrameTable();
int  allocatePageTable(int pid);
void initializePageTable(int pt_frame);
int  handlePageReplacement(int pid, int vpn);
int  translateAddress(int pid, int virtual_address, char rw);
void freeprocessframes(int pid);
void resetReferencedBits();
bool isModified(int frame_index);
void checkPendingMemoryLogs();

#endif