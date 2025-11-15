#ifndef __TASK_TRACE_SUPERVISOR_MONITOR_METRICS_H__
#define __TASK_TRACE_SUPERVISOR_MONITOR_METRICS_H__

#define MAX_NUMBER_OF_PERFORMANCE_MARKERS   1

#include <stdint.h>

typedef struct {
    uint64_t lastCyclicTaskReadyTime;   // Use to save the tick when the cyclic (deadline) task will be ready to run again
    int64_t totalAltoAdjust;
    int64_t maxLatency;    // The max amount of time taken for the cyclic task to run after being in the ready state
    int64_t minLatency;    // The min amount of time taken for the cyclic task to run after being in the ready state
    int64_t lastLatency;   // The last amount of time taken for the cyclic task to run after being in the ready state
    uint32_t deadlineLostCount;     // Count the amount of lost deadlines
    uint32_t runtimeOverrunCount;   // Count the amount of time the task overrun its runtime
    uint32_t taskDepletedCount;     // Count the amount of time the task overrun its runtime and was "throttled" or "depleted"
    uint64_t cycleCount;   // How many periods have elapsed

    uint64_t lastStartRunTime; // Used to save the time of when user task started its work
    uint64_t lastStopRunTime;  // Used to save the time of when user task finished its work
    
    uint64_t lastRT;        // Response Time
    uint64_t minRT;         // The minimum Response time
    uint64_t WCRT;          // Worst Case Response Time    (maximum Response time)

    uint64_t perfMarkStart[MAX_NUMBER_OF_PERFORMANCE_MARKERS];
    uint64_t perfMarkStop[MAX_NUMBER_OF_PERFORMANCE_MARKERS];
    uint64_t perfMartTotalTime[MAX_NUMBER_OF_PERFORMANCE_MARKERS];
} MonitorMetrics;

void MonitorMetrics_init(MonitorMetrics *const me);

#endif // __TASK_TRACE_SUPERVISOR_MONITOR_METRICS_H__