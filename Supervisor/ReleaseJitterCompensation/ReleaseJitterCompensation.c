#include "ReleaseJitterCompensation.h"
#include "../../TaskTrace/TaskTrace.h"
#include "../Monitor/Monitor.h"

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static ReleaseJitterCompensation compensation = 
{
    .pid = 0, 
    .estimatedReleaseJitter = 0,
    .teoricalPeriodRelease = 0,

    .deadlineInNs = 100 * 1000,   // reserved runtime = 500us, but we shouldn't use it
    .periodInNs = 10 * 1000 * 1000 // period = 50 ms
};

pid_t ReleaseJitterCompensation_getTaskPID(void)
{
    return compensation.pid;
}

uint64_t ReleaseJitterCompensation_getValueToCompensate(void)
{
    return compensation.estimatedReleaseJitter;
}


/**
 * @brief Task used for release deadline jitter estimation and error compensation from linux kernel
 *      scheduling strategy
 * 
 * @param arg 
 * @return void* 
 */
void *ReleaseJitterCompensation_task(void *arg)
{
    (void) arg;
    TaskTrace taskTrace;
    compensation.pid = PID_get();
    printf("ReleaseJitterCompensation_task: Creating (pid = %d)\n", compensation.pid);

    if (TaskTrace_init(&taskTrace)) {
        printf("ReleaseJitterCompensation_task: Error initializing taskTrace\n");
        return NULL;
    }

    if (TaskTrace_enableRecording(&taskTrace)) {
        printf("ReleaseJitterCompensation_task: Start recording returned error\n");
        return NULL;
    }
    
    if (PID_setDeadline(compensation.pid, 
                        compensation.deadlineInNs, 
                        compensation.deadlineInNs, 
                        compensation.periodInNs)) {
        printf("ReleaseJitterCompensation_task: Error changing scheduler to deadline");
        perror("");
        return NULL;
    }

    while(1) {
        // we do not need the start and stop, we just need one timestamp here, so I call only
        // the execution stop, since its the one that send the data to the monitor task
        // So, the monitor task will ignore the value of the execution start timestamp and consider
        // the execution stop as the start
        TaskTrace_traceExecutionStop(&taskTrace);

        // wait for a new period
        sched_yield();
    }

    return NULL;
}

void *ReleaseJitterCompensation_monitorTask(void *arg)
{
    MonitorThread *const me = arg;
    
    printf("ReleaseJitterCompensation_monitorTask: Task created\n");
    
    SharedMem_supervisorInit(&me->sharedMem, me->pid);

    Monitor_init(me);

    while(1) {
        ssize_t numberOfBytesReceived = SharedMem_supervisorRead(&me->sharedMem, &me->telegram);

        if (numberOfBytesReceived != sizeof(Telegram)) {
            if (numberOfBytesReceived == 0) {
                if (PID_checkIfExist(me->pid != 1)) {
                    // User task was closed
                    pthread_exit(NULL);
                }
            }
            printf("ReleaseJitterCompensation_monitorTask: invalid telegram, received %ld bytes (should be %ld)\n", numberOfBytesReceived, sizeof(Telegram));
            continue;
        }

        if (me->pid != me->telegram.pid) {
            printf("ReleaseJitterCompensation_monitorTask: invalid telegram, pid %d received (should be %d)\n", me->telegram.pid, me->pid);
            continue;
        }

        switch (me->telegram.code)
        {
        case TelegramCode_startAndStopTime:
            uint64_t *thisTaskStartTime = &me->metrics.lastStartRunTime;
            uint64_t *lastTaskStartTime = &me->metrics.lastCyclicTaskReadyTime;
            uint64_t *lastRealPeriod = &me->metrics.lastRT;
            int64_t *jitter = &me->metrics.lastLatency;

            // me->telegram.t1 is not filled cause we don't ned it
            *thisTaskStartTime = me->telegram.t2;

            do {
                // Calculate jitter
                // If first cicle we just wait for the next
                if (*lastTaskStartTime == 0) {
                    *lastTaskStartTime = *thisTaskStartTime;
                    compensation.teoricalPeriodRelease = *thisTaskStartTime;
                    break;
                }

                compensation.teoricalPeriodRelease += compensation.periodInNs;

                *lastRealPeriod = *thisTaskStartTime - *lastTaskStartTime;
                *jitter = *lastRealPeriod - compensation.periodInNs;
                compensation.estimatedReleaseJitter = *jitter;
                // if (*thisTaskStartTime < compensation.teoricalPeriodRelease) {
                //     compensation.teoricalPeriodRelease = *thisTaskStartTime;
                // }
                int64_t absoluteError = (*thisTaskStartTime - compensation.teoricalPeriodRelease);

                printf("ReleaseJitterCompensation_monitorTask: startTime = %lu, lastPeriod = %lu, jitter = %ld, absoluteError = %ld\n", 
                    *lastTaskStartTime/1000, 
                    *lastRealPeriod/1000, 
                    *jitter/1000, 
                    (absoluteError)/1000);

                if (me->enableLogging) {
                    me->telegram.t1 = me->telegram.t2;
                    Log_addNewPoint(&me->log, &me->telegram, &me->metrics, false);
                    Log_generateIfNeeded(&me->log);
                }
                
                *lastTaskStartTime = *thisTaskStartTime;
            } while(0);

            break;
        default:
            // printf("Monitor %d: Error, invalid code (%d) received\n", me->pid, me->telegram.code);
            break;
        }

        // printf("Monitor %d: Telegram received successfully: \n", me->pid);
        // printf("       pid:      %d\n", me->telegram[0].pid);
        // printf("       code:     %d\n", me->telegram[0].code);
        // printf("       time(ns): %ld\n", me->lastStartRunTime);
    }

    
    return NULL;
}