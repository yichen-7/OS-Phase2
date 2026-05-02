#include "headers.h"
bool process_finished = false;
// \ global to capture finish time precisely
int finished_process_pid = -1;
int finish_recorded_time = -1;

void handle_sigusr2(int sig) {
    if (sig == SIGUSR2) {
        process_finished = true;
        finish_recorded_time = getClk(); // capture NOW, not later
    }
}


//!============================== Priority Queue Implementation ==============================!//
typedef struct Node
{
    struct PCB process;
    struct Node* next;
} Node;

Node *head = NULL;
bool isqueueEmpty()
{
    return head == NULL;
}

bool enqueue(struct PCB process)
{
    Node* newNode = (Node*)malloc(sizeof(Node));
    newNode->process = process;
    newNode->next = NULL;

    // if (isqueueEmpty() || head->process.priority > process.priority)
    // Tie-breaker for the head: Insert if new process has higher priority (lower number)
    // OR if priorities are equal but the new process has a smaller ID.
    if (isqueueEmpty() || 
        process.priority < head->process.priority || 
        (process.priority == head->process.priority && process.id < head->process.id))
    {
        newNode->next = head;
        head = newNode;
        return true;
    }

    Node* current = head;
    // Traverse the queue:
    // Move forward if the next process has a higher priority (lower value)
    // OR if the next process has the same priority but its ID is still smaller than the new one
    while (current->next != NULL && 
          (current->next->process.priority < process.priority || 
          (current->next->process.priority == process.priority && current->next->process.id < process.id)))
    {
        current = current->next;
    }
    newNode->next = current->next;
    current->next = newNode;
    return true;
}
bool dequeue(struct PCB* process)
{
    if (isqueueEmpty())
    {
        return false;
    }
    Node* temp = head;
    *process = head->process;
    head = head->next;
    free(temp);
    return true;
}
struct PCB peek()
{
    if (isqueueEmpty())
    {
        struct PCB emptyPCB;
        emptyPCB.id = -1; // Indicate an empty PCB
        return emptyPCB;
    }
    return head->process;
}
//!============================== End of Priority Queue Implementation ==============================!//

bool enqueue_rr(struct PCB process)
{
    Node* newNode = (Node*)malloc(sizeof(Node));
    newNode->process = process;
    newNode->next = NULL;

    if (isqueueEmpty())
    {
        newNode->next = head;
        head = newNode;
        return true;
    }

    Node* current = head;
    while (current->next != NULL)
    {
        current = current->next;
    }
    newNode->next = current->next;
    current->next = newNode;
    return true;
}


// File pointers for output
FILE *log_file;
FILE *perf_file;

//for calculating average waiting time and turnaround time
int total_waiting_time = 0;
int total_execution_time = 0;
int last_finish_time = 0;
int first_start_time = 0;
float total_wta = 0;
float total_wta_sq = 0;
int total_processes_count = 0;

// Function to calculate the current waiting time of a process
int get_current_wait(struct PCB* p) {
    return (getClk() - p->arrival_time) - p->time_executed;
}


void print_process_stats(struct PCB* p, int finish_time) {
    int TAT = finish_time - p->arrival_time;
    int WT = TAT - p->runtime;
    float current_wta = (float)TAT / p->runtime; 

    // Update totals for average waiting time and standard deviation calculations
    total_wta += current_wta;
    total_wta_sq += (current_wta * current_wta);
    total_processes_count++;
    total_waiting_time += WT;
    total_execution_time += p->runtime;
    last_finish_time = finish_time;

    // Log the 'finished' state
    fprintf(log_file, "At time %d process %d finished arr %d total %d remain 0 wait %d TA %d WTA %.2f\n", 
            finish_time, p->id, p->arrival_time, p->runtime, WT, TAT, current_wta);
}


float get_std_wta() {
    if (total_processes_count == 0) return 0.0;
    
    double avg_wta = total_wta / total_processes_count;
    
    double variance = (total_wta_sq / total_processes_count) - (avg_wta * avg_wta);

    if (variance < 0) variance = 0;
    
    return sqrt(variance);
}

// Finalizes scheduler by reporting metrics and cleaning resources
void finalize_report() {
    if (total_processes_count > 0 && last_finish_time > 0) {
        float avg_wta = total_wta / total_processes_count;
        float avg_waiting = (float)total_waiting_time / total_processes_count;
        //float utilization = ((float)total_execution_time / last_finish_time) * 100.0;
        float utilization = ((float)total_execution_time / (last_finish_time - first_start_time)) * 100.0;
        
        fprintf(perf_file, "CPU utilization = %.2f%%\n", utilization);
        fprintf(perf_file, "Avg WTA = %.2f\n", avg_wta);
        fprintf(perf_file, "Avg Waiting = %.2f\n", avg_waiting);
        fprintf(perf_file, "Std WTA = %.2f\n", get_std_wta());
    }
    
    // Close files and cleanup resources [cite: 742, 799]
    fclose(log_file);
    fclose(perf_file);
    printf("10 seconds idle. Termination complete.\n");
    destroyClk(true);
    exit(0);
}



void print_process(struct PCB process) {
    printf("Process ID: %d\n", process.id);
    printf("Arrival Time: %d\n", process.arrival_time);
    printf("Runtime: %d\n", process.runtime);
    printf("Remaining Time: %d\n", process.remaining_time);
    printf("Priority: %d\n", process.priority);
    printf("State: %d\n", process.state);
}




// Helper function to write in log file standard states (Started, Resumed, Stopped)
    void log_process_state(const char* state, struct PCB* p) 
    {
    if (log_file == NULL || p == NULL) return;
    fprintf(log_file, "At time %d process %d %s arr %d total %d remain %d wait %d\n", 
            getClk(), p->id, state, p->arrival_time, 
            p->runtime, p->remaining_time, get_current_wait(p));
    fflush(log_file); // <--- IMPORTANT: This forces writing to the file immediately
    }






struct message schedulerMsg;


int main(int argc, char * argv[])
{
    initClk();

    int cpu_ready_time = 0; //For adding the 1 second overhead of context switching

    int turnoff_timer = -1; // To track when to destroy the clock and exit the scheduler (RR)
    int Algorithm = 1;
    int quantum = 0;
    int shift_needed = 0;

 
    
    //Read the user's choice from the process generator
    if (argc > 1) {
        Algorithm = atoi(argv[1]);
    }
    if (argc > 2) { 
        quantum = atoi(argv[2]); //Assuming 2nd argument is the quantum for RR
    }
    if (argc > 3) {
        shift_needed = atoi(argv[3]); //Assuming 3rd argument is the shift flag
    }

    // Open output files in write mode -- this will create the files if they don't exist and overwrite them if they do - 'w' mode is for writing
        log_file = fopen("scheduler.log", "w");
        perf_file = fopen("scheduler.perf", "w");

        // if (shift_needed) {
        //     fprintf(log_file, "# Note: One of the processes had an arrival time of 0. All arrival times were shifted by +1 to avoid simultaneous arrival with clock start.\n");
        // }
        
        // Write header for the log file
        if (log_file != NULL) {
            fprintf(log_file, "#At time x process y state arr w total z remain y wait k\n");
        }


    
    printf("Scheduler started at time %d\n", getClk());
    signal(SIGUSR2, handle_sigusr2);
    struct PCB* current_process = NULL;
    
    //TODO implement the scheduler :(
   int msgid = msgget(MSGKEY, 0666 | IPC_CREAT);
   struct message schedulerMsg;



   while(1)
   {             static int last_printed_time = -1;
                if (getClk() != last_printed_time) {
                    // 1. Check if we are currently in the 1-second overhead period
                    if (getClk() < cpu_ready_time) 
                    {
                        // Highlighting Context Switching in Yellow
                        printf("\033[1;33m[Clock: %d] == Context Switching... (Overhead)\033[0m\n", getClk());
                        } 
                    // 2. Check if a process is actually running
                    else if (current_process != NULL) {
                        printf("[Clock: %d] ## CPU is busy with Process %d\n", getClk(), current_process->id);
                    } 
                    // 3. Otherwise, CPU is idle
                    else {
                        printf("[Clock: %d] .. CPU is idle\n", getClk());
                    }
                    fflush(stdout);
                    last_printed_time = getClk();
                    }



        int rec_val = msgrcv(msgid, &schedulerMsg, sizeof(struct message)-sizeof(long), 0, IPC_NOWAIT);
        if (rec_val != -1)
        {
            struct PCB newpcb;
            newpcb.id = schedulerMsg.p.id;
            newpcb.arrival_time = schedulerMsg.p.arrival;
            newpcb.runtime = schedulerMsg.p.runtime;
            newpcb.remaining_time = schedulerMsg.p.runtime;
            newpcb.priority = schedulerMsg.p.priority;
            newpcb.state = STATE_ARRIVED;
            newpcb.time_executed = 0;

            // Terminal log for arrival
            printf("[Clock: %d] >> Process %d arrived (Priority: %d, Runtime: %d)\n",getClk(), newpcb.id, newpcb.priority, newpcb.runtime);  

            // printf("Scheduler received process with \nID: %d\t arrival time: %d\n runtime: %d\t priority: %d\n", schedulerMsg.p.id, schedulerMsg.p.arrival, schedulerMsg.p.runtime, schedulerMsg.p.priority);
            //log_process_state("arrived", &newpcb);
            if (Algorithm==1)
            {
                enqueue(newpcb);

                    if(current_process != NULL)
                {
                    if(newpcb.priority < current_process->priority && current_process->remaining_time > 0 && !process_finished) //&& !process_finished added this 47min before the submission by ghrbr to handle the case where a process finishes at the same time a new one arrives with higher priority.
                     //In that case, we don't want to preempt a process that just finished.
                    {
                        //preempt the current process
                        
                        int time_spent = getClk() - current_process->start_time;
                        if (time_spent > 0)
                        {
                        current_process->remaining_time -= time_spent; 
                        current_process->time_executed += time_spent; 
                        }

                        //usleep(250000);
                        if(!process_finished)
                        {
                        kill(current_process->system_pid, SIGSTOP);
                        current_process->state = STATE_STOPPED;
                        log_process_state("stopped", current_process);
                        enqueue(*current_process);
                        // printf("Process %d PREEMPTED by process %d\n", current_process->id, newpcb.id);
                        // Terminal log for preemption
                        printf("[Clock: %d] !! Preemption: Process %d paused for higher priority Process %d\n", getClk(), current_process->id, newpcb.id);
                        
                        free(current_process);
                        current_process = NULL;
                        cpu_ready_time = getClk() + 1; 
                        }
                        
                    }
                }
            }
            if (Algorithm == 2) 
            {
                enqueue_rr(newpcb);
            }
        } // <================ REC_VAL BRACKET CLOSES HERE! =================>
        
        
        
            if (Algorithm == 1){



                
            // if (process_finished) {
            //     // printf("Process finished at time %d\n", getClk());
            //     log_process_state("finished", current_process);
            //     // int TAT = getClk() - current_process->arrival_time;
            //     // int WT = TAT - current_process->runtime;
            //     // float WTAT = (float)TAT / current_process->runtime;
            //     // printf("Waiting Time (WT) for process %d: %d\n", current_process->id, WT);
            //     // printf("Turnaround Time (TAT) for process %d: %d\n", current_process->id, TAT);
            //     // printf("Weighted Turnaround Time (WTAT) for process %d: %.2f\n", current_process->id, WTAT);

            //     print_process_stats(current_process);

            //     process_finished = false; // reset the flag for the next process
            //     free(current_process); // free the memory allocated for the current process
            //     current_process = NULL; // set the pointer to NULL after freeing
            //     cpu_ready_time = getClk() + 1; // Add context switch overhead
            // }
                if (process_finished && current_process != NULL) 
                { // <--- ALWAYS check for NULL
                    //log_process_state("finished", current_process);
                    printf("[Clock: %d] OK Process %d finished execution\n", getClk(), current_process->id);
                    print_process_stats(current_process, finish_recorded_time);
                    
                    process_finished = false;
                    free(current_process);
                    current_process = NULL;

                    if (!isqueueEmpty()) {
                        cpu_ready_time = getClk() + 1; // Only add overhead if there's a process waiting to be switched TO
                    }
                    else cpu_ready_time = getClk(); // CPU becomes idle, next arrival starts instantly
                            
                    
                }

                // Added (getClk() >= cpu_ready_time) to respect the context switching overhead
                if (current_process == NULL && !isqueueEmpty() && getClk() >= cpu_ready_time) {
                struct PCB new_process;
                dequeue(&new_process);
                current_process = (struct PCB*)malloc(sizeof(struct PCB));
                *current_process = new_process;
                if(current_process->state == STATE_ARRIVED)
                {
                    int pid = fork();
                    if (pid == 0) 
                        {
                            // In child process : excute the process file
                            char remaining_time_str[20];
                            char id_str[20];
                            sprintf(id_str, "%d", current_process->id);
                            sprintf(remaining_time_str, "%d", current_process->remaining_time);
                            execl("./process.out", "./process.out", remaining_time_str, id_str, NULL);

                        }
                        else 
                        {
                            // In parent process : update the PCB and print the start time
                            current_process->system_pid = pid;
                            current_process->state = STATE_STARTED;
                            current_process->start_time = getClk();
                            if (first_start_time == 0) first_start_time = getClk();
                            // printf("Process %d started at time %d\n", current_process->id, getClk());
                            log_process_state("started", current_process);
                        }
                    }
                    else if(current_process->state == STATE_STOPPED)
                    {
                        kill(current_process->system_pid, SIGCONT);
                        current_process->state = STATE_RESUMED;
                        current_process->start_time = getClk();
                        printf("[Clock: %d] -> CPU resumed Process %d (Remaining: %d)\n",getClk(), current_process->id, current_process->remaining_time);
                        // printf("Time %d: Process %d resumed. Remaining: %d\n", getClk(), current_process->id, current_process->remaining_time);
                        log_process_state("resumed", current_process);
                    }
                
            
                }

            }

            if (Algorithm == 2) //RR
            {
                int time_spent;
                if (current_process!=NULL){
                time_spent = getClk() - current_process->start_time;
                }

               
                if (  current_process != NULL && process_finished) { 

                    current_process->remaining_time = 0;
                    current_process->time_executed += time_spent;
                    //log_process_state("finished", current_process);
                    printf("Time %d: Process %d executed for %d units. Remaining: %d\n", getClk(), current_process->id, time_spent, current_process->remaining_time);
                    process_finished = false; // reset the flag for the next process
                    print_process_stats(current_process, finish_recorded_time);
                    free(current_process);        
                    current_process = NULL;
                    if(!isqueueEmpty())
                    {cpu_ready_time = getClk() + 1;} // Add context switch overhead}
                    else cpu_ready_time = getClk(); // No overhead if CPU goes idle
                    
                    

                }
               
               
              else if ((time_spent >= quantum ) && !process_finished && current_process != NULL && current_process->remaining_time > 0) {

                    if (current_process->remaining_time > quantum) {
                    current_process->remaining_time -= quantum;
                    current_process->time_executed += quantum; // Updated time executed with the quantum, not the actual time spent, to reflect the intended execution slice
                    
                    
                    bool other_waiting = !isqueueEmpty(); //Checking before enqueueing the stopped process to see if there are other processes waiting. This will determine if we apply the overhead for context switching or not.
                                                          //For handling the case where a process finishes exactly at the quantum expiry, we check the process_finished flag before applying the overhead.
                                                          //  If the process finished, we skip the overhead since there will be no next process to switch to. 
                                                          // If it didn't finish, we apply the overhead as usual.

                    

                    if (other_waiting) {
                    kill(current_process->system_pid, SIGSTOP);
                    log_process_state("stopped", current_process);
                    current_process->state = STATE_STOPPED;
                    } 


                    

                    enqueue_rr(*current_process);
                    free(current_process);
                    current_process = NULL;

                    if (other_waiting) {
                    cpu_ready_time = getClk() + 1; // different process next — apply overhead
                    }
                    else {
                    cpu_ready_time = getClk(); // same process resumes — no overhead
                    }
                    
                }

                else {
                    // If queue is empty, don't stop. Just "renew" the quantum.
                     // We update time_executed but keep the process running.
                    
                    int actual_execution = (current_process->remaining_time > quantum) ? quantum : current_process->remaining_time;
                        current_process->remaining_time -= actual_execution;
                        current_process->time_executed += actual_execution;
                        current_process->start_time = getClk();
                        // If it still has time, it just keeps going. 
                        // If it reached 0, the process_finished flag from the signal will handle it.
                    // current_process->remaining_time = 0;
                    // current_process->start_time = getClk();
                    // if (first_start_time == 0) first_start_time = getClk();


                    // kill(current_process->system_pid, SIGCONT);
                    // current_process->state = STATE_RESUMED;
                    // current_process->start_time = getClk();
                    // log_process_state("resumed", current_process);
                // current_process->time_executed += current_process->remaining_time;
                // current_process->remaining_time = 0;
                // kill(current_process->system_pid, SIGKILL);
                // printf("[Clock: %d] Process %d finished via quantum expiry.\n", getClk(), current_process->id);
                // print_process_stats(current_process, getClk());
                // free(current_process);
                // current_process = NULL;
                // cpu_ready_time = getClk() + 1;
    }

            }
               

                if (current_process == NULL && !isqueueEmpty() && getClk() >= cpu_ready_time) { // The cooldown period must pass for the code to continue (this is the 1 second penalty for context switching)
                    struct PCB new_process;
                    dequeue(&new_process); 
                    
                    current_process = (struct PCB*)malloc(sizeof(struct PCB));
                    *current_process = new_process;
                    
                    if(current_process->state == STATE_ARRIVED)
                    {
                        int pid = fork();
                        if (pid == 0) {
                            char remaining_time_str[20], id_str[20];
                            sprintf(id_str, "%d", current_process->id);
                            sprintf(remaining_time_str, "%d", current_process->remaining_time);
                            execl("./process.out", "./process.out", remaining_time_str, id_str, NULL);
                        } else {
                            current_process->system_pid = pid;
                            current_process->state = STATE_STARTED;
                            current_process->start_time = getClk();
                            if (first_start_time == 0) first_start_time = getClk();
                            log_process_state("started", current_process);
                            printf("Process %d started at time %d\n", current_process->id, getClk());
                        }
                    }
                    else if(current_process->state == STATE_STOPPED)
                    {
                        kill(current_process->system_pid, SIGCONT);
                        current_process->state = STATE_RESUMED;
                        current_process->start_time = getClk();
                        log_process_state("resumed", current_process);
                        printf("Time %d: Process %d resumed. Remaining: %d\n", getClk(), current_process->id, current_process->remaining_time);
                    }
                }


            }


            if (current_process == NULL && isqueueEmpty()) {
                 if (turnoff_timer == -1) {
                    turnoff_timer = getClk(); // start the turnoff timer
                        } else if (getClk() - turnoff_timer >= 10) 
                        { // 10 seconds of doing nothing
                        // printf("Standard Deviation of WTA: %.2f\n", get_std_wta());
                        // printf("10 seconds passed with no work. Terminating.\n");
                            finalize_report(); // Report and exit
                        destroyClk(true);
                        exit(0);
                    }
                    } 
                    else {
                    turnoff_timer = -1; // reset timer if there's work to do
            }
            // // Handle termination after 10 seconds of idleness
            //     if (current_process == NULL && isqueueEmpty()) 
            //     {
            //         if (turnoff_timer == -1) 
            //             turnoff_timer = getClk(); // Start idle timer
            //     }
            //     else if (getClk() - turnoff_timer >= 10)  
            //     { // If idle for 10 seconds, finalize report and exit
            //             finalize_report(); // Report and exit
            //     } 
            //     else 
            //     {
            //         turnoff_timer = -1; // Reset timer if there's work to do
            //     }

                        
    } // <================== END OF WHILE(1) LOOP ==================>
            destroyClk(true);
            
               return 0;
}