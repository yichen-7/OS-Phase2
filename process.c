#include "headers.h"

int remainingtime;
int runtime;
int lasttime;

void continue_handler(int sig)
{
    lasttime = getClk();
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

    int id = 0;
    if (argc > 2)
        id = atoi(argv[2]);

    runtime = remainingtime;

    initClk();

    // Read request file for this process
    struct Request requests[100];
    int requestCount = 0;
    int nextRequest = 0;

    char reqFileName[50];
    sprintf(reqFileName, "requests_%d.txt", id);
    FILE *reqFile = fopen(reqFileName, "r");
    if (reqFile != NULL)
    {
        char line[100];
        while (fgets(line, sizeof(line), reqFile) != NULL)
        {
            if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
            int t;
            char addr[20];
            char rw;
            sscanf(line, "%d %s %c", &t, addr, &rw);
            requests[requestCount].time = t;
            requests[requestCount].address = (int)strtol(addr, NULL, 0);
            requests[requestCount].actiontype = rw;
            requestCount++;
        }
        fclose(reqFile);
    }

    int msgid = msgget(MSGKEY, 0666 | IPC_CREAT);

    lasttime = getClk();

    while (remainingtime > 0)
    {
        int current_time = getClk();
        if (current_time > lasttime)
        {
            remainingtime--;
            lasttime = current_time;

            int time_executed = runtime - remainingtime;

            // Fire any requests whose time matches current CPU time consumed
            while (nextRequest < requestCount && requests[nextRequest].time < time_executed)
            {
                struct RequestMessage reqMsg;
                reqMsg.mtype = 2;
                reqMsg.pid = id;
                reqMsg.address = requests[nextRequest].address;
                reqMsg.actiontype = requests[nextRequest].actiontype;
                msgsnd(msgid, &reqMsg, sizeof(struct RequestMessage) - sizeof(long), 0);
                nextRequest++;
            }
        }
    }

    kill(getppid(), SIGUSR2);
    destroyClk(false);
    return 0;
}
