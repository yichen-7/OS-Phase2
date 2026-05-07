#include "MMU.h"
#include "headers.h"

static FrameEntry frameTable[NUM_FRAMES];

extern struct PCB* getProcessById(int pid);// from scheduler.c by orashy
extern void blockProcess(int pid, int page_number); // from scheduler.c by orashy

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
        // printf("DEBUG: fault detected for pid %d\n", pid);
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

int handlePageReplacement(int pid, int vpn) {
    int delay = 10; //defualt delay for loading a page into RAM
    int target_frame = -1;

    // check free frames
    for (int i = 0; i < NUM_FRAMES; i++) {
        if (frameTable[i].is_free) {
            target_frame = i;
            break;
        }
    }
    
    // evict if full
    if (target_frame == -1) {
        target_frame = selectVictimFrame();
        
        // printf("DEBUG: evicting frame %d\n", target_frame);
        
        if (isModified(target_frame)) {
            delay = 20; 
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
    frameTable[target_frame].R = 0; 
    frameTable[target_frame].M = 0;
    frameTable[target_frame].is_page_table = 0; 

    // link pte
    PageTableEntry *new_pte = getPageTableEntry(pid, vpn);
    new_pte->valid = 1;
    new_pte->frame_number = target_frame;
    new_pte->R = 0;
    new_pte->M = 0;

    return delay;
}

int allocatePageTable(int pid) {
    for (int i = 0; i < NUM_FRAMES; i++) {
        if (frameTable[i].is_free) {
            frameTable[i].is_free = 0;
            frameTable[i].pid = pid;
            frameTable[i].is_page_table = 1; 
            frameTable[i].R = 0;
            frameTable[i].M = 0;
            return i; 
        }
    }
    return -1; 
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