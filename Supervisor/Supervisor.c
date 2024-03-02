#include "Supervisor.h"
#include "../Common/Process/Process.h"

#include <stdio.h>
#include <string.h>
#include <dirent.h>     // opendir, readdir, closedir
#include <stdlib.h>     // atoi
#include <sys/mman.h>   // mlockall
#include <unistd.h>     // usleep, getuid

#include <ncurses.h>    //  Interface

static void *Supervisor_checkAndCleanUnusedSharedMemThread(void *arg);
static void *Monitor_task(void *arg);
static void *Supervisor_interfaceUpdateTask(void *arg);

static int Supervisor_reserveInstance(Supervisor *const me);
static int Supervisor_freeInstance(Supervisor *const me, int instanceNumber);
static int Supervisor_checkIfTaskIsBeingTraced(Supervisor *const me, pid_t pid);

int main() 
{
    Supervisor supervisor;

    if (geteuid() != 0) {
        printf("Supervisor: Error, permission denied. Run as sudo.\n");
        return -1;
    }

    printf("Supervisor: Starting configuration\n");
    if (Supervisor_init(&supervisor)) {
        printf("Supervisor: Error initiallizing\n");
        return -2;
    }

    printf("Supervisor: Creating cleanup task\n");
    if (pthread_create(&supervisor.cleanUpTask_id, NULL, Supervisor_checkAndCleanUnusedSharedMemThread, &supervisor) != 0) {
        perror("Supervisor: Error creating cleanup task");
        return -3;
    }

    sleep(1);   // Wait for the cleanup task to make the first clean.

    printf("Supervisor: Creating interface update task\n");
    if (pthread_create(&supervisor.interfaceUpdateTask_id, NULL, Supervisor_interfaceUpdateTask, &supervisor) != 0) {
        perror("Supervisor: Error creating interface update task");
        return -4;
    }

    while(1) {
        //printf("Supervisor: Checking for new tasks to trace\n");
        pid_t pid = Supervisor_checkNewTaskToTrace(&supervisor);
        //printf("Supervisor: new task to trace found with pid_t %d\n", (int) pid);

        int instanceNumber;
        //printf("Supervisor: checking for a free monitoring instace\n");
        while ((instanceNumber = Supervisor_reserveInstance(&supervisor)) < 0) {
            usleep(500000);
        }
       // printf("Supervisor: Free instance %d found\n", instanceNumber);

        supervisor.monitor[instanceNumber].pid = pid;

        //printf("Supervisor: Creating a thread to monitor the user process %d\n", pid);
        if (pthread_create(&supervisor.monitor[instanceNumber].id, 
                NULL, 
                Monitor_task, 
                &supervisor.monitor[instanceNumber]) != 0) {
            perror("Supervisor: Error creating monitoring thread");
            return -5;
        }
    }

    return 0;
}

int Supervisor_init(Supervisor *const me)
{
    // config stack em heap to stay always allocated and avoid page faults during operation
    mlockall(MCL_CURRENT | MCL_FUTURE);

    memset(me, 0, sizeof(Supervisor));

    if (pthread_mutex_init(&me->isTaskBeingTraced_mutex, NULL) != 0) {
        return -1;
    }

    // Adjust max and minumum values
    for (ssize_t i = 0; i < MAX_TRACED_TASKS; i++) {
        me->monitor[i].metrics.minWorkTime = (uint64_t) -1;
        me->monitor[i].metrics.maxWorkTime = 0;
    }

    return 0;
}

pid_t Supervisor_checkNewTaskToTrace(Supervisor *const me)
{
    int newTaskToTraceFound = 0;
    pid_t pid;

    DIR *dir;

    // Open the default shared mem region to search on
    while((dir = opendir(SharedMem_getDefaultPath())) == NULL);

    do {
        struct dirent *entry;
        usleep(50000);  //50ms
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

/**
 * @brief Each instance of this task is responsible for monitoring one user process.
 * 
 * @param arg Pointer to the sharedMem to monitor.
 * @return void* 
 */
static void *Monitor_task(void *arg)
{
    MonitorThread *const me = arg;
    
    printf("Supervisor: Monitor task for pid %d created\n", me->pid);
    
    // 1º Open the shared mem as read only
    SharedMem_supervisorInit(&me->sharedMem, me->pid);

    while(1) {
        ssize_t numberOfBytesReceived = SharedMem_supervisorRead(&me->sharedMem, &me->telegram);

        if (numberOfBytesReceived != sizeof(Telegram)) {
            printf("Monitor %d: invalid telegram, received %ld bytes (should be %ld)\n", me->pid, numberOfBytesReceived, sizeof(Telegram));
            continue;
        }

        if (me->pid != me->telegram.pid) {
            printf("Monitor %d: invalid telegram, pid %d received (should be %d)\n", me->pid, me->telegram.pid, me->pid);
            continue;
        }

        switch (me->telegram.code)
        {
        case TelegramCode_startWorkPoint:
            me->metrics.lastStartWorkTime = (me->telegram.timestamp.tv_sec * 1000 * 1000 * 1000) + (me->telegram.timestamp.tv_nsec);
            break;
        case TelegramCode_stopWorkPoint:
            me->metrics.lastStopWorkTime = (me->telegram.timestamp.tv_sec * 1000 * 1000 * 1000) + (me->telegram.timestamp.tv_nsec);

            // Calculate the work time
            me->metrics.lastWorkTime = me->metrics.lastStopWorkTime - me->metrics.lastStartWorkTime;

            // Calculate the maximum and minimum values
            if (me->metrics.lastWorkTime > me->metrics.maxWorkTime) {
                me->metrics.maxWorkTime = me->metrics.lastWorkTime;
            } else if (me->metrics.lastWorkTime < me->metrics.minWorkTime) {
                me->metrics.minWorkTime = me->metrics.lastWorkTime;
            }

            printf("Monitor %d: total work time = %ld ns\n", me->pid, me->metrics.lastWorkTime);
            break;
        case TelegramCode_perfMark1Start:
            me->metrics.perfMarkStart[TelegramCode_perfMark1Start - TELEGRAM_PERFMARK_OFFSET] = 
                (me->telegram.timestamp.tv_sec * 1000 * 1000 * 1000) + (me->telegram.timestamp.tv_nsec);
            break;
        case TelegramCode_perfMark1End:
            me->metrics.perfMarkStop[TelegramCode_perfMark1End - TELEGRAM_PERFMARK_OFFSET] = 
                (me->telegram.timestamp.tv_sec * 1000 * 1000 * 1000) + (me->telegram.timestamp.tv_nsec);
            me->metrics.perfMartTotalTime[TelegramCode_perfMark1End - TELEGRAM_PERFMARK_OFFSET - 1] = 
                me->metrics.perfMarkStop[TelegramCode_perfMark1End - TELEGRAM_PERFMARK_OFFSET - 1] - me->metrics.perfMarkStart[0];
            printf("Monitor %d: performance mark %d work time = %ld ns\n", me->pid, TelegramCode_perfMark1Start - TELEGRAM_PERFMARK_OFFSET + 1, me->metrics.lastWorkTime);
            break;
        default:
            printf("Monitor %d: Error, invalid code (%d) received\n", me->pid, me->telegram.code);
            break;
        }

        // printf("Monitor %d: Telegram received successfully: \n", me->pid);
        // printf("       pid:      %d\n", me->telegram[0].pid);
        // printf("       code:     %d\n", me->telegram[0].code);
        // printf("       time(ns): %ld\n", me->lastStartWorkTime);
    }

    pthread_exit(NULL);
}


static void *Supervisor_interfaceUpdateTask(void *arg)
{
    Supervisor *const me = arg;

    printf("Supervisor: Interface updater task created\n");

    while (1) {

    }

    pthread_exit(NULL);
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