#include "Supervisor.h"
#include "../Common/Process/Process.h"

#include <stdio.h>
#include <string.h>
#include <dirent.h>     // opendir, readdir, closedir
#include <stdlib.h>     // atoi
#include <sys/mman.h>
#include <unistd.h>     // usleep

static void *Supervisor_checkAndCleanUnusedSharedMemThread(void *arg);
static void *Supervisor_userProcessMonitorTask(void *arg);

static int Supervisor_getFirstFreeInstance(Supervisor *const me);

int main() 
{
    Supervisor supervisor;

    printf("Supervisor: Starting configuration\n");
    if (Supervisor_init(&supervisor)) {
        printf("Supervisor: Error initiallizing\n");
        return -1;
    }

    printf("Supervisor: Creating cleanup task\n");
    if (pthread_create(&supervisor.cleanUpTask_id, NULL, Supervisor_checkAndCleanUnusedSharedMemThread, &supervisor) != 0) {
        perror("Supervisor: Error creating cleanup task");
        return -2;
    }

    while(1) {
        printf("Supervisor: Checking for new tasks to trace\n");
        PID pid = Supervisor_checkNewTaskToTrace(&supervisor);
        printf("Supervisor: new task to trace found with PID %d\n", (int) pid);

        int instanceNumber;
        printf("Supervisor: checking for a free monitoring instace\n");
        while ((instanceNumber = Supervisor_getFirstFreeInstance(&supervisor)) < 0) {
            usleep(500000); 
        }
        printf("Supervisor: Free instance %d found\n", instanceNumber);

        supervisor.isTaskBeingTraced[instanceNumber] = 1;  // Reserve the instance
        supervisor.sharedMem[instanceNumber].pid = pid;

        printf("Supervisor: Creating a thread to monitor the user process %d\n", pid);
        if (pthread_create(&supervisor.supervisorTask_id[instanceNumber], 
                NULL, 
                Supervisor_userProcessMonitorTask, 
                &supervisor.sharedMem[instanceNumber]) != 0) {
            perror("Supervisor: Error creating monitoring thread");
            return -3;
        }
    }

    return 0;
}

int Supervisor_init(Supervisor *const me)
{
    // config stack em heap to stay always allocated and avoid page faults during operation
    mlockall(MCL_CURRENT | MCL_FUTURE);

    memset(me, 0, sizeof(Supervisor));

    return 0;
}

PID Supervisor_checkNewTaskToTrace(Supervisor *const me)
{
    int newTaskToTraceFound = 0;
    PID pid;

    DIR *dir;

    // Open the default shared mem region to search on
    while((dir = opendir(SharedMem_getDefaultPath())) == NULL);

    do {
        struct dirent *entry;
        usleep(50000);  //50ms
        rewinddir(dir);

        // Check if there is any new region created by some task that wants to be traced
        while ((entry = readdir(dir)) != NULL) {
            //printf("Supervisor_checkNewTaskToTrace: entry->d_name |%s|, PID = %d\n", entry->d_name, atoi(entry->d_name));
            if ((pid = (PID) atoi(entry->d_name)) <= 0) {
                continue;
            }
            int taskAlreadyBeingTraced = 0;

            for (size_t i = 0; i < MAX_TRACED_TASKS; i++) {
                if ((me->isTaskBeingTraced[i]) && (me->sharedMem[i].pid == pid)) {
                    taskAlreadyBeingTraced = 1;
                    break;
                }
            }
            
            if (taskAlreadyBeingTraced == 0) {
                newTaskToTraceFound = 1;
                break;
            }
            sleep(1);
        }
    } while (newTaskToTraceFound == 0);

    // Close the default shared mem region
    closedir(dir);

    return pid;
}

static void *Supervisor_checkAndCleanUnusedSharedMemThread(void *arg)
{
    (void) arg; // To avoid a warning
    DIR *dir;

    // Open the default shared mem region to search on
    while((dir = opendir(SharedMem_getDefaultPath())) == NULL);

    while (1) {
        struct dirent *entry;
        sleep(10);

        // Check if there is a region created that does not correspont to any process
        while ((entry = readdir(dir)) != NULL) {
            PID pid = (PID) atoi(entry->d_name);
            
            if (PID_checkIfExist(pid) == 0) {
                remove(entry->d_name);
            }
        }
        
        rewinddir(dir);
    }

    // Close the default shared mem region
    closedir(dir);

    pthread_exit(NULL);
}

/**
 * @brief Each instance of this task is responsible for monitoring one user process.
 * 
 * @param arg Pointer to the sharedMem to monitor.
 * @return void* 
 */
static void *Supervisor_userProcessMonitorTask(void *arg)
{
    SharedMem *const sharedMem = arg;
    (void) sharedMem; //to avoid a warning for now
    // Here, we open to read the shared mem and keep reading until the user process is finished
    // 1º Open the shared mem as read only
    while(1) {
        sleep(1);
    }

    pthread_exit(NULL);
}

/**
 * @brief Search a free instance to be used to monitor a new user process
 * 
 * @param me 
 * @return int The instance number. -1 if no free instance.
 */
static int Supervisor_getFirstFreeInstance(Supervisor *const me)
{
    int instanceNumber;
    for (instanceNumber = 0; instanceNumber < MAX_TRACED_TASKS; instanceNumber++) {
        if (me->isTaskBeingTraced[instanceNumber] == 0) {
            return instanceNumber;
        }
    }

    return -1;  // No instance free
}