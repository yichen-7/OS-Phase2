#include "MMU.h"
//#include "headers.h"

static FrameEntry frameTable[NUM_FRAMES];
FILE *memory_log;

extern struct PCB* getProcessById(int pid);// from scheduler.c by orashy
extern void blockProcess(int pid, int page_number); // from scheduler.c by orashy

void initMemoryLog() {
    memory_log = fopen("memory.log", "w");
}

// Structure to store delayed memory log messages
struct PendingLog {
    int time;
    int disk_addr;
    int pid;
    int frame;
    int active;
} pending_logs[100];
int active_logs_count = 0;


void initializeFrameTable() {// called once at system startup to set all frames as free and clear metadata
    for (int i = 0; i < NUM_FRAMES; i++) {
        frameTable[i].is_free = 1;
        frameTable[i].pid = -1; 
        frameTable[i].virtual_page = -1; 
        frameTable[i].is_page_table = 0; 
        frameTable[i].R = 0; 
        frameTable[i].M = 0; 
    }
}

PageTableEntry* getPageTableEntry(int pid, int page_number) {
    struct PCB *process = getProcessById(pid);
    if (!process) return NULL;
    
    int pt_frame = process->page_table_frame;
    if (pt_frame == -1) return NULL;

    // map physical address
    PageTableEntry *table_base = (PageTableEntry *)((char *)ram_shmaddr + (pt_frame * PAGE_SIZE));
    return &table_base[page_number];
}

int translateAddress(int pid, int virtual_address, char rw) {
    int page_number = virtual_address / PAGE_SIZE; 
    int offset = virtual_address % PAGE_SIZE;

    PageTableEntry *pte = getPageTableEntry(pid, page_number);
    
    if (pte->valid == 0) {
        fprintf(memory_log, "PageFault upon VA 0x%x from process %d\n", virtual_address, pid);
        fflush(memory_log);
        blockProcess(pid, page_number);
        return -1;
    }

    // sync NRU bits
    pte->R = 1; 
    frameTable[pte->frame_number].R = 1; 
    
    if (rw == 'w' || rw == 'W') { // write access
        pte->M = 1; 
        frameTable[pte->frame_number].M = 1; 
    }

    return (pte->frame_number * PAGE_SIZE) + offset; 
}

void freeprocessframes(int pid){ // called when process finishes to free all frames it owns //!(including page table)
    for (int i = 0; i < NUM_FRAMES; i++) {
        if (frameTable[i].pid == pid) {
            frameTable[i].is_free = 1; 
            frameTable[i].pid = -1; 
            frameTable[i].virtual_page = -1; 
            frameTable[i].is_page_table = 0; 
            frameTable[i].R = 0; 
            frameTable[i].M = 0; 
        }
    }
}

int selectVictimFrame() {//
   int target_class;
   for (int c = 0; c < 4; c++){
       for (int i = 0; i < NUM_FRAMES; i++) {
           if(frameTable[i].is_page_table || frameTable[i].is_free) continue;
             
           int R = frameTable[i].R;
           int M = frameTable[i].M;
           target_class = (R*2) + M; // class 0: R=0 M=0, class 1: R=0 M=1, class 2: R=1 M=0, class 3: R=1 M=1 //*binary logic
           
           if (target_class == c) {
               return i; 
           }
       }
   }
   return -1; 
}

void resetReferencedBits() {//helper func
    for (int i = 0; i < NUM_FRAMES; i++) {
        frameTable[i].R = 0;
    }
}

bool isModified(int frame_index) { //helper func
    return frameTable[frame_index].M == 1;
}



// Called by the scheduler every tick to print delayed memory logs
void checkPendingMemoryLogs() {
    if (active_logs_count == 0) return;

    int current_time = getClk();
    for(int i = 0; i < 100; i++) {
        if(pending_logs[i].active && pending_logs[i].time == current_time) {
            fprintf(memory_log, "At time %d disk address %d for process %d is loaded into memory page %d.\n",
                    current_time, pending_logs[i].disk_addr, pending_logs[i].pid, pending_logs[i].frame);
            fflush(memory_log);
            pending_logs[i].active = 0; // Mark log as inactive
            active_logs_count--;
        }
    }
}

int handlePageReplacement(int pid, int vpn) {
    // Check if this is the initial page load for the process
    int is_initial = 1;
    for (int i = 0; i < NUM_FRAMES; i++) {
        // If a data page exists, it is not the initial load
        if (frameTable[i].pid == pid && frameTable[i].is_page_table == 0) {
            is_initial = 0;
            break;
        }
    }

    int delay = is_initial ? 0 : 10;
    int target_frame = -1;

    // check free frames
    for (int i = 0; i < NUM_FRAMES; i++) {
        if (frameTable[i].is_free) {
            target_frame = i;
            break;
        }
    }

    if (target_frame != -1) {
        fprintf(memory_log, "Free Physical page %d allocated\n", target_frame);
        fflush(memory_log);
    } else {
        // RAM full, page fault
        delay = 10; 
        target_frame = selectVictimFrame();

        if (isModified(target_frame)) {
            delay = 20;
            fprintf(memory_log, "Swapping out page %d to disk\n", target_frame);
            fflush(memory_log);
        }

        // invalidate old owner
        int old_pid = frameTable[target_frame].pid;
        int old_vpn = frameTable[target_frame].virtual_page;
        PageTableEntry *old_pte = getPageTableEntry(old_pid, old_vpn);

        if (old_pte) {
            old_pte->valid = 0;
            old_pte->frame_number = -1;
        }
    }

    // assign new owner
    frameTable[target_frame].is_free = 0;
    frameTable[target_frame].pid = pid;
    frameTable[target_frame].virtual_page = vpn;
    frameTable[target_frame].R = 1; 
    frameTable[target_frame].M = 0;
    frameTable[target_frame].is_page_table = 0;

    PageTableEntry *new_pte = getPageTableEntry(pid, vpn);
    new_pte->valid = 1;
    new_pte->frame_number = target_frame;
    new_pte->R = 1; 
    new_pte->M = 0;

    struct PCB *process = getProcessById(pid);
    int disk_address = process->base + vpn;
    
    if (delay == 0) {
        // Print immediately for initial load (no delay)
        fprintf(memory_log, "At time %d disk address %d for process %d is loaded into memory page %d.\n",
                getClk(), disk_address, pid, target_frame);
        fflush(memory_log);
    } else {
        // Store log to be printed by the scheduler after disk delay
        for(int i = 0; i < 100; i++) {
            if(!pending_logs[i].active) {
                pending_logs[i].time = getClk() + delay;
                pending_logs[i].disk_addr = disk_address;
                pending_logs[i].pid = pid;
                pending_logs[i].frame = target_frame;
                active_logs_count++;
                pending_logs[i].active = 1;
                break;
            }
        }
    }

    return delay;
}


int allocatePageTable(int pid) {
    // Try to find a free frame
    for (int i = 0; i < NUM_FRAMES; i++) {
        if (frameTable[i].is_free) {
            frameTable[i].is_free = 0;
            frameTable[i].pid = pid;
            frameTable[i].is_page_table = 1; 
            frameTable[i].R = 0;
            frameTable[i].M = 0;
            fprintf(memory_log, "Free Physical page %d allocated\n", i);
            return i; 
        }
    }
    
    int target_frame = selectVictimFrame();
    
    // Invalidate the old owner's Page Table Entry
    int old_pid = frameTable[target_frame].pid;
    int old_vpn = frameTable[target_frame].virtual_page;
    PageTableEntry *old_pte = getPageTableEntry(old_pid, old_vpn);

    if (old_pte) {
        old_pte->valid = 0;
        old_pte->frame_number = -1;
    }

    // Assign the frame to the new process as a Page Table
    frameTable[target_frame].is_free = 0;
    frameTable[target_frame].pid = pid;
    frameTable[target_frame].is_page_table = 1; // Protects it from future eviction
    frameTable[target_frame].R = 0;
    frameTable[target_frame].M = 0;
    
    fprintf(memory_log, "Free Physical page %d allocated\n", target_frame);
    return target_frame; 
}


void initializePageTable(int pt_frame) {
    char *pt_ptr = (char *)ram_shmaddr + (pt_frame * PAGE_SIZE);
    
    // clear memory segment
    for (int i = 0; i < PAGE_SIZE; i++) {
        pt_ptr[i] = 0;
    }
}

int findPageTableFrame(int pid) {
    for (int i = 0; i < NUM_FRAMES; i++) {
        if (frameTable[i].is_page_table && frameTable[i].pid == pid) {
            return i;
        }
    }
    return -1; 
}