#ifndef __TASK_TRACE_SUPERVISOR_MONITOR_H__
#define __TASK_TRACE_SUPERVISOR_MONITOR_H__

#define MAX_NUMBER_OF_PERFORMANCE_MARKERS   1

#include "../../Common/SharedMem/SharedMem.h"
#include "MonitorMetrics/MonitorMetrics.h"

#include <pthread.h>

#include "../Log/Log.h"

#ifndef SCHED_DEADLINE
#define SCHED_DEADLINE  6
#endif

typedef struct {
    pthread_t id;   // thread id
    pid_t pid;        // user pid that is being monitored by this thread
    int policy;         // Used to know if the task that is being monitored is a deadline task or other type
    SharedMem sharedMem;    // The memory shared between the user process and the monitor process
    Telegram telegram;    // to be used as rx buffer

    MonitorMetrics metrics;

    Log log;
    bool enableLogging;
} MonitorThread;

void Monitor_init(MonitorThread *const me);
void *Monitor_task(void *arg);

#endif // __TASK_TRACE_SUPERVISOR_MONITOR_H__