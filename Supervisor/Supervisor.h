#ifndef __TASK_TRACE_SUPERVISOR_H__
#define __TASK_TRACE_SUPERVISOR_H__

#include "../Common/SharedMem/SharedMem.h"

#include <pthread.h>

#define MAX_TRACED_TASKS 100
#define MAX_NUMBER_OF_PERFORMANCE_MARKERS   1

typedef struct {
    uint64_t lastStartWorkTime; // Used to save the time of when user task started its work
    uint64_t lastStopWorkTime;  // Used to save the time of when user task finished its work
    uint64_t lastWorkTime;  // The amount of time that the user task spent until the end of its work

    uint64_t maxWorkTime;   // The maximum time spent by the user task to finish its work
    uint64_t minWorkTime;   // The minimum time spent by the user task to finish its work

    uint64_t perfMarkStart[MAX_NUMBER_OF_PERFORMANCE_MARKERS];
    uint64_t perfMarkStop[MAX_NUMBER_OF_PERFORMANCE_MARKERS];
    uint64_t perfMartTotalTime[MAX_NUMBER_OF_PERFORMANCE_MARKERS];
} MonitorMetrics;

typedef struct {
    pthread_t id;   // thread id
    pid_t pid;        // user pid that is being monitored by this thread
    SharedMem sharedMem;    // The memory shared between the user process and the monitor process
    Telegram telegram;    // to be used as rx buffer

    MonitorMetrics metrics;
} MonitorThread;

typedef struct {
    int isEnabled;  // To enable/disable the autotuning of deadline tasks
    int policy;     
} Autotune;

/**
 * @brief Struct with all possible user inputs
 * 
 */
typedef struct {
    uint64_t systemLatency;
    Autotune autotune;
} UserInputs;

typedef struct {
    uint64_t systemLatency;     // The system latency in us, must be passed by the user. USer should use some tools to find it (RTLA, for example)
    UserInputs userInputs;
    int isTaskBeingTraced[MAX_TRACED_TASKS];    // Used to know which instances are being used to trace a user task
    pthread_mutex_t isTaskBeingTraced_mutex;    // Used to protect the list of task being traced

    pthread_t cleanUpTask_id;
    pthread_t interfaceUpdateTask_id;
    MonitorThread monitor[MAX_TRACED_TASKS];
} Supervisor;

int Supervisor_init(Supervisor *const me, const UserInputs *const userInputs);
pid_t Supervisor_checkNewTaskToTrace(Supervisor *const me);

#endif // __TASK_TRACE_SUPERVISOR_H__