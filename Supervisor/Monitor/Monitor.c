#include "Monitor.h"

#include <string.h>
#include <stdio.h>

void Monitor_init(MonitorThread *const me)
{
    me->policy = PID_getSchedulerPolicy(me->pid, NULL);

    MonitorMetrics_init(&me->metrics);

    char pidAsString[10];
    sprintf(pidAsString, "%d", me->pid);

    Log_init(&me->log, "/tmp/taskTrace/", pidAsString);

    if (me->enableLogging) {
        Log_enable(&me->log);
    } else {
        Log_disable(&me->log);
    }
}

/**
 * @brief Each instance of this task is responsible for monitoring one user process.
 * 
 * @param arg Pointer to the sharedMem to monitor.
 * @return void* 
 */
void *Monitor_task(void *arg)
{
    MonitorThread *const me = arg;
    
    printf("Supervisor: Monitor task for pid %d created\n", me->pid);
    
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
            printf("Monitor %d: invalid telegram, received %ld bytes (should be %ld)\n", me->pid, numberOfBytesReceived, sizeof(Telegram));
            continue;
        }

        if (me->pid != me->telegram.pid) {
            printf("Monitor %d: invalid telegram, pid %d received (should be %d)\n", me->pid, me->telegram.pid, me->pid);
            continue;
        }

        switch (me->telegram.code)
        {
        case TelegramCode_startAndStopTime:
            me->metrics.lastStartRunTime = me->telegram.t1;
            me->metrics.lastStopRunTime = me->telegram.t2;

            // Calculate the execution time
            me->metrics.lastRT = me->metrics.lastStopRunTime - me->metrics.lastStartRunTime;

            // Calculate the maximum and minimum execution time values
            if (me->metrics.lastRT > me->metrics.WCRT) {
                me->metrics.WCRT = me->metrics.lastRT;
            } else if (me->metrics.lastRT < me->metrics.minRT) {
                me->metrics.minRT = me->metrics.lastRT;
            }

            do {
                if (me->policy == SCHED_DEADLINE) {
                    // Read task deadline parameters
                    uint64_t runtime, period, deadline;
                    if (PID_getDeadlinePropeties(me->pid, &runtime, &deadline, &period) != 0) {
                        me->policy = PID_getSchedulerPolicy(me->pid, NULL);
                        if (me->policy != SCHED_DEADLINE) {
                            printf("Monitor %d: Error, not able to read deadline properties cause user changed the policy to %d\n", me->pid, me->policy);
                        } else {
                            printf("Monitor %d: Error, not able to read deadline properties\n", me->pid);
                        }
                        break;
                    }

                    bool exceptionDetected = false;

                    /*
                        I try to estimate using the trace point of the start exect time
                        This might work we cause I also have a autoadjust, that will stay tuning this value forever.
                    */
                    if (me->metrics.lastCyclicTaskReadyTime == 0) {
                        me->metrics.lastCyclicTaskReadyTime = me->metrics.lastStartRunTime;
                    }

                    // Autoajust the ready tick in case the first tick was not 100% correct
                    if (me->metrics.lastStartRunTime < me->metrics.lastCyclicTaskReadyTime) {
                        me->metrics.totalAltoAdjust += me->metrics.lastCyclicTaskReadyTime - me->metrics.lastStartRunTime;
                        me->metrics.lastCyclicTaskReadyTime = me->metrics.lastStartRunTime;
                    }

                    // Calculate latency
                    me->metrics.lastLatency = (int64_t) me->metrics.lastStartRunTime - me->metrics.lastCyclicTaskReadyTime;

                    if (me->metrics.lastLatency > me->metrics.maxLatency) {
                        me->metrics.maxLatency = me->metrics.lastLatency;
                    } else if (me->metrics.lastLatency < me->metrics.minLatency) {
                        me->metrics.minLatency = me->metrics.lastLatency;
                    }

                    // Check for deadline lost
                    if ((me->metrics.lastStopRunTime - me->metrics.lastCyclicTaskReadyTime) > deadline) {
                        exceptionDetected = true;
                        me->metrics.deadlineLostCount++;
                    }
                    // if (me->metrics.lastRT + me->metrics.lastLatency > deadline) {
                    //     me->metrics.deadlineLostCount++;
                    // }

                    // Check for task runtime overruns
                    if (me->metrics.lastRT > runtime) {
                        exceptionDetected = true;
                        me->metrics.runtimeOverrunCount++;

                        // Check for task "throttled" or "depleted" in case of overruns
                        if (me->metrics.lastRT > period) {
                            me->metrics.taskDepletedCount++;
                        }
                    }
                    
                    // Update next tick where the task will be ready
                    // Here I have a while cause there could have happend some runtime overflows
                    while(me->metrics.lastCyclicTaskReadyTime < me->metrics.lastStopRunTime) {
                        me->metrics.lastCyclicTaskReadyTime += period;
                        me->metrics.cycleCount++;
                    }

                    if (me->enableLogging) {
                        Log_addNewPoint(&me->log, &me->telegram, &me->metrics, exceptionDetected);
                        Log_generateIfNeeded(&me->log);
                    }
                }
            } while(0);

            break;
        case TelegramCode_perfMark1Start:
            me->metrics.perfMarkStart[TelegramCode_perfMark1Start - TELEGRAM_PERFMARK_OFFSET] = me->telegram.t1;
            break;
        case TelegramCode_perfMark1End:
            me->metrics.perfMarkStop[TelegramCode_perfMark1End - TELEGRAM_PERFMARK_OFFSET] = me->telegram.t1;
            me->metrics.perfMartTotalTime[TelegramCode_perfMark1End - TELEGRAM_PERFMARK_OFFSET - 1] = 
                me->metrics.perfMarkStop[TelegramCode_perfMark1End - TELEGRAM_PERFMARK_OFFSET - 1] - 
                me->metrics.perfMarkStart[TelegramCode_perfMark1End - TELEGRAM_PERFMARK_OFFSET - 1];
            // printf("Monitor %d: performance mark %d work time = %ld ns\n", me->pid, TelegramCode_perfMark1Start - TELEGRAM_PERFMARK_OFFSET + 1, me->metrics.lastExecutionTime);
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

    pthread_exit(NULL);
}
