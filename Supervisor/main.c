#include "Supervisor.h"
#include "TerminalInterface/TerminalInterface.h"
#include "UserInputs/UserInputs.h"

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

static Supervisor supervisor;
static TerminalInterface terminalInterface;
static UserInputs userInputs;

int main(int argc, char *argv[]) 
{
    printf("Main: Reading user inputs\n");
    if (UserInputs_read(&userInputs, argc, argv)) {
        return -1;
    }

    printf("Main: Starting supervisor\n");
    if (Supervisor_init(&supervisor, &userInputs)) {
        return -2;
    }

    printf("Main: Starting terminal interface\n");
    if (TerminalInterface_start(&terminalInterface, &supervisor)) {
        printf("Error starting terminal interface\n");
    }

    pthread_join(supervisor.checkTask_id, NULL);

    return 0;
}