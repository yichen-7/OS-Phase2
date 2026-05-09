#ifndef MMU_H
#define MMU_H

// Core memory specs
#define RAM_SIZE 512
#define PAGE_SIZE 16
#define NUM_FRAMES (RAM_SIZE / PAGE_SIZE) 

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

// Function Prototypes - Your specific tasks
int translateAddress(int pid, int virtual_address, char rw);
void freeProcessFrames(int pid);

#endif