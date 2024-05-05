#ifndef __TASK_TRACE_SUPERVISOR_H__
#define __TASK_TRACE_SUPERVISOR_H__

#include "../Common/SharedMem/SharedMem.h"

#include <pthread.h>

#define MAX_TRACED_TASKS 100
#define MAX_NUMBER_OF_PERFORMANCE_MARKERS   1

typedef struct {
    uint64_t lastCyclicTaskReadyTime;   // Use to save the tick when the cyclic (deadline) task will be ready to run again
    uint64_t maxLatency;    // The max amount of time taken for the cyclic task to run after being in the ready state
    uint64_t minLatency;    // The min amount of time taken for the cyclic task to run after being in the ready state
    uint64_t lastLatency;   // The last amount of time taken for the cyclic task to run after being in the ready state
    uint32_t deadlineLostCount;     // Count the amount of lost deadlines
    uint32_t runtimeOverrunCount;   // Count the amount of time the task overrun its runtime
    uint32_t taskDepletedCount;     // Count the amount of time the task overrun its runtime and was "throttled" or "depleted"

    uint64_t lastStartExecutionTime; // Used to save the time of when user task started its work
    uint64_t lastStopExecutionTime;  // Used to save the time of when user task finished its work
    
    uint64_t lastET;            // Execution time
    uint64_t minET;   // The minimum Execution time
    uint64_t WCET;          // Worst Case Execution Time    (maximum execution time)

    uint64_t perfMarkStart[MAX_NUMBER_OF_PERFORMANCE_MARKERS];
    uint64_t perfMarkStop[MAX_NUMBER_OF_PERFORMANCE_MARKERS];
    uint64_t perfMartTotalTime[MAX_NUMBER_OF_PERFORMANCE_MARKERS];
} MonitorMetrics;

typedef struct {
    pthread_t id;   // thread id
    pid_t pid;        // user pid that is being monitored by this thread
    int policy;         // Used to know if the task that is being monitored is a deadline task or other type
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

#endif // __TASK_TRACE_SUPERVISOR_H__