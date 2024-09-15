#include "Supervisor.h"
#include "../Common/Process/Process.h"

#include <stdio.h>
#include <string.h>
#include <dirent.h>     // opendir, readdir, closedir
#include <stdlib.h>     // atoi
#include <sys/mman.h>   // mlockall
#include <unistd.h>     // usleep, getuid

static pid_t Supervisor_checkNewTaskToTrace(Supervisor *const me);
static int Supervisor_reserveInstance(Supervisor *const me);
static int Supervisor_freeInstance(Supervisor *const me, int instanceNumber);
static int Supervisor_checkIfTaskIsBeingTraced(Supervisor *const me, pid_t pid);

static void *Supervisor_checkForNewProcessesThread(void *arg);
static void *Supervisor_checkAndCleanUnusedSharedMemThread(void *arg);

int Supervisor_init(Supervisor *const me, const UserInputs *const userInputs)
{
    if (geteuid() != 0) {
        printf("Supervisor: Error, permission denied. Run as sudo.\n");
        return -1;
    }

    // config stack em heap to stay always allocated and avoid page faults during operation
    mlockall(MCL_CURRENT | MCL_FUTURE);

    memset(me, 0, sizeof(Supervisor));

    if (pthread_mutex_init(&me->isTaskBeingTraced_mutex, NULL) != 0) {
        return -2;
    }

    memcpy(&me->userInputs, userInputs, sizeof(UserInputs));

    printf("Supervisor: Creating cleanup task\n");
    if (pthread_create(&me->cleanUpTask_id, NULL, Supervisor_checkAndCleanUnusedSharedMemThread, me) != 0) {
        perror("Supervisor: Error creating cleanup task");
        return -3;
    }

    printf("Supervisor: Creating task that check for new user processes \n");
    if (pthread_create(&me->checkTask_id, NULL, Supervisor_checkForNewProcessesThread, me) != 0) {
        perror("Supervisor: Error creating task that check for new processes");
        return -4;
    }

    return 0;
}

static pid_t Supervisor_checkNewTaskToTrace(Supervisor *const me)
{
    int newTaskToTraceFound = 0;
    pid_t pid;

    DIR *dir;

    // Open the default shared mem region to search on
    while((dir = opendir(SharedMem_getDefaultPath())) == NULL);

    do {
        struct dirent *entry;
        usleep(500000);  //500ms
        rewinddir(dir);

        // Check if there is any new region created by some task that wants to be traced
        while ((entry = readdir(dir)) != NULL) {
            //printf("Supervisor_checkNewTaskToTrace: entry->d_name |%s|, pid_t = %d\n", entry->d_name, atoi(entry->d_name));
            if ((pid = (pid_t) atoi(entry->d_name)) <= 0) {
                continue;
            }

            int taskAlreadyBeingTraced = Supervisor_checkIfTaskIsBeingTraced(me, pid);
            
            if (taskAlreadyBeingTraced == 0) {
                newTaskToTraceFound = 1;
                break;
            }
        }
    } while (newTaskToTraceFound == 0);

    // Close the default shared mem region
    closedir(dir);

    return pid;
}

/**
 * @brief Reserve the first free instance found
 * 
 * @param me 
 * @return int The instance number. -1 if no free instance.
 */
static int Supervisor_reserveInstance(Supervisor *const me)
{
    int instanceNumber;
    int freeInstanceFound = 0;

    pthread_mutex_lock(&me->isTaskBeingTraced_mutex);
    for (instanceNumber = 0; instanceNumber < MAX_TRACED_TASKS; instanceNumber++) {
        if (me->isTaskBeingTraced[instanceNumber] == 0) {
            me->isTaskBeingTraced[instanceNumber] = 1;
            freeInstanceFound = 1;
            break;
        }
    }
    pthread_mutex_unlock(&me->isTaskBeingTraced_mutex);

    if (freeInstanceFound) {
        return instanceNumber;
    }

    return -1;  // No free instance found
}

static int Supervisor_freeInstance(Supervisor *const me, int instanceNumber)
{
    pthread_mutex_lock(&me->isTaskBeingTraced_mutex);
    if (me->isTaskBeingTraced[instanceNumber] == 0) {
        pthread_mutex_unlock(&me->isTaskBeingTraced_mutex);
        return -1;  //instance not being used, we should not call a free() to a already freed instance
    }

    me->isTaskBeingTraced[instanceNumber] = 0;
    pthread_mutex_unlock(&me->isTaskBeingTraced_mutex);

    return 0;
}

/**
 * @brief Check if the task is already being traced
 * 
 * @param me 
 * @param pid the pid that you want to check if is already being traced
 * @return int 
 */
static int Supervisor_checkIfTaskIsBeingTraced(Supervisor *const me, pid_t pid)
{
    int isTaskBeingTraced = 0;
    pthread_mutex_lock(&me->isTaskBeingTraced_mutex);
    for (int i = 0; i < MAX_TRACED_TASKS; i++) {
        if (me->isTaskBeingTraced[i] && (me->monitor[i].pid == pid)) {
            isTaskBeingTraced = 1;
            break;
        }
    }
    pthread_mutex_unlock(&me->isTaskBeingTraced_mutex);

    return isTaskBeingTraced;
}

static void *Supervisor_checkForNewProcessesThread(void *arg)
{
    Supervisor *const me = arg;

    while(1) {
        //printf("Supervisor: Checking for new tasks to trace\n");
        pid_t pid = Supervisor_checkNewTaskToTrace(me);
        //printf("Supervisor: new task to trace found with pid_t %d\n", (int) pid);

        int instanceNumber;
        //printf("Supervisor: checking for a free monitoring instace\n");
        while ((instanceNumber = Supervisor_reserveInstance(me)) < 0) {
            usleep(500000);
        }
        // printf("Supervisor: Free instance %d found\n", instanceNumber);

        me->monitor[instanceNumber].pid = pid;
        me->monitor[instanceNumber].enableLogging = me->userInputs.enableLogging;

        //printf("Supervisor: Creating a thread to monitor the user process %d\n", pid);
        if (pthread_create(&me->monitor[instanceNumber].id, 
                NULL, 
                Monitor_task, 
                &me->monitor[instanceNumber]) != 0) {
            perror("Supervisor: Error creating monitoring thread");
            printf("Supervisor: PID %d will not be supervised\n", pid);
        }
    }

    pthread_exit(NULL);
}

static void *Supervisor_checkAndCleanUnusedSharedMemThread(void *arg)
{
    Supervisor *const me = arg;
    DIR *dir;

    printf("Supervisor: Cleanup task created\n");
    // Open the default shared mem region to search on
    while((dir = opendir(SharedMem_getDefaultPath())) == NULL);

    while (1) {
        struct dirent *entry;

        // Check if there is a region created that does not correspont to any process
        while ((entry = readdir(dir)) != NULL) {
            pid_t pid = (pid_t) atoi(entry->d_name);
            // printf("Cleanup: entry->d_name = %s, pid = %d\n", entry->d_name, pid);
            
            if (pid <= 0) {
                continue;
            }

            // Do not remove memories that as still being monitored, even if the user process doesn't exist anymore.
            if (Supervisor_checkIfTaskIsBeingTraced(me, pid)) {
                continue;
            }

            if (PID_checkIfExist(pid) == 0) {
                char path[50];
                SharedMem_getFinalPath(path, pid);
                if (remove(path)) {
                    perror("Supervisor: Cleanup removing error\n");
                }
            }
        }
        
        rewinddir(dir);

        sleep(10);
    }

    // Close the default shared mem region
    closedir(dir);

    pthread_exit(NULL);
}
