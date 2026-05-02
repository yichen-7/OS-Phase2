#include "headers.h"

void clearResources(int);

int msgid;

// Comparison function for qsort
int compareProcesses(const void* a, const void* b) {
    struct process* p1 = (struct process*)a;
    struct process* p2 = (struct process*)b;

    // 1. Primary sort by Arrival Time
    if (p1->arrival != p2->arrival) {
        return p1->arrival - p2->arrival;
    }
    // 2. Secondary sort by ID (Tie-breaker for same arrival)
    return p1->id - p2->id;
}


int main(int argc, char * argv[])
{
    signal(SIGINT, clearResources);
    // TODO Initialization
      char* fileName;
  if (argc > 1) {
          fileName = argv[1]; // Use the argument if provided
      } else {
          fileName = "process.txt"; // Default filename
          printf("No file provided, using default: process.txt\n");
      } 


    // 1. Read the input files.
    FILE *file = fopen(fileName, "r");

if (file == NULL) {
        perror("FATAL ERROR"); // This prints the exact Linux system error
        printf("Could not find the file named: '%s'\n", fileName);
        printf("Check your folder to ensure it isn't named process.txt.txt\n");
        exit(1); // Safely kill the program instead of segfaulting
    }

  char line[100];
  struct process processes[100]; 
    int processCount = 0;

  while (fgets(line, sizeof(line), file) != NULL) {
      if (line[0] =='#' || line[0] == '\n' || line[0] == '\r')   {
            continue; 
      }
      else {    
        
        int id, arrival, runtime, priority;
      sscanf(line, "%d%d%d%d", &id, &arrival, &runtime, &priority);

        if (runtime <= 0) {
            printf("\033[1;33mWarning: Process %d ignored because runtime is 0.\033[0m\n", id);
            continue;
        }
        processes[processCount].id = id;
        processes[processCount].arrival = arrival;
        processes[processCount].runtime = runtime;
        processes[processCount].priority = priority;
        processes[processCount].remainingTime = runtime;
        processCount++;

      }
    }
    fclose(file);
    // Sort processes by arrival time (and ID as tie-breaker)
    qsort(processes, processCount, sizeof(struct process), compareProcesses);

    bool shift_needed = false;
    for (int i = 0; i < processCount; i++) {
        if (processes[i].arrival == 0) {
            shift_needed = true;
            break;
        }
    }

    // If someone arrived at 0, shift EVERYONE'S arrival time by 1
    if (shift_needed) {
        for (int i = 0; i < processCount; i++) {
            processes[i].arrival += 1;
        }
        // \033[1;31m is the code for Bold Red
        // \033[0m resets the color back to normal
        printf("\033[1;31mNote: A process arrived at time 0. All arrival times have been shifted by +1 to avoid simultaneous arrival with clock start.\033[0m\n");
    }

    // Convert the boolean flag to a string so we can pass it to the scheduler
    char shift_str[10];
    sprintf(shift_str, "%d", shift_needed ? 1 : 0);


    // 2. Ask the user for the chosen scheduling algorithm and its parameters, if there are any.
    printf("Please choose the scheduling algorithm you want to use:\n");
    printf("1. HPF\n");
    printf("2. RR\n");
    int schedulingAlgorithm;
    scanf("%d",&schedulingAlgorithm);
    int quantumTime = 0;
   if ( schedulingAlgorithm == 2) {
        printf("Enter the time quantum for RR (must be a positive integer): ");
        
        // Loop continues if scanf fails to read an integer OR if the integer is <= 0
        while (scanf("%d", &quantumTime) != 1 || quantumTime <= 0) {
            printf("Invalid input! Please enter a strictly positive integer: ");
            
            // Clear the input buffer. This prevents an infinite loop if the user types a letter like 'z'
            while (getchar() != '\n'); 
        }
    }
    char algo_str[10];
  char quantum_str[10];
  sprintf(algo_str, "%d", schedulingAlgorithm);      
  sprintf(quantum_str, "%d", quantumTime);            

    // 3. Initiate and create the scheduler and clock processes.
    int clockProcess = fork();
    if (clockProcess == 0) {
        execl("./clk.out", "./clk.out", NULL);
    }

     //sleep(2);
    int schedulerpid = fork();
    if (schedulerpid == 0) {
        execl("./scheduler.out", "./scheduler.out", algo_str, quantum_str, shift_str, NULL);
    }


   
    // 4. Use this function after creating the clock process to initialize clock
    initClk();
    // To get time use this
    int x = getClk();
    printf("current time is %d\n", x);
    // TODO Generation Main Loop

     msgid = msgget(MSGKEY, 0666 | IPC_CREAT);
    int i = 0;
    while (i < processCount) {
        int currentTime = getClk();
        while(i < processCount && processes[i].arrival <= currentTime) {
            struct message msg;
            msg.mtype = 1;
            msg.p = processes[i];
            msgsnd(msgid, &msg, sizeof(struct process), 0);

            i++;
            
        }
    }





    // 5. Create a data structure for processes and provide it with its parameters.
    // 6. Send the information to the scheduler at the appropriate time.
    // 7. Clear clock resources
    waitpid(schedulerpid, NULL, 0);
    destroyClk(true);
}

void clearResources(int signum)
{
    //TODO Clears all resources in case of interruption
     msgctl(msgid, IPC_RMID, NULL);  
      destroyClk(true);              
      exit(0);
}
