#include "headers.h"

/* Modify this file as needed*/
int remainingtime;

int lasttime;

void continue_handler(int sig)
{
    
        //printf("Process %d received SIGCONT signal, resuming execution.\n", getpid());
        lasttime = getClk(); // Update lasttime to the current clock time when resuming
    
}

int main(int argc, char * argv[])
{

    signal(SIGCONT, continue_handler);

    if (argc > 1)
    {
        remainingtime = atoi(argv[1]);
    }
    else {
        printf("Error: No remaining time provided for the process.\n");
        return 1;
    }
    initClk();
    
    //TODO it needs to get the remaining time from somewhere
    //remainingtime = ??;


     lasttime = getClk();
     
    while (remainingtime > 0)
    {
        int current_time = getClk();
        if (current_time > lasttime) {
            remainingtime--;
            lasttime = current_time;
            //printf("[Process %d] remaining time: %d\n", getpid(), remainingtime);
            //printf("[Process %d] remaining time: %d at time %d\n", atoi(argv[2]), remainingtime, current_time);
        }
        
        // remainingtime = ??;
    }
    kill(getppid(), SIGUSR2); //notify the scheduler that the process has finished
    
    destroyClk(false);
    
    return 0;
}
