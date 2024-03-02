#ifndef __TASK_TRACE_SUPERVISOR_H__
#define __TASK_TRACE_SUPERVISOR_H__

#include "../Common/SharedMem/SharedMem.h"

#include <pthread.h>

#define MAX_TRACED_TASKS 10
#define MAX_MONITOR_BUFFER 64*1024

typedef struct {
    pthread_t id;   // thread id
    pid_t pid;        // user pid that is being monitored by this thread
    SharedMem sharedMem;    // The memory shared between the user process and the monitor process
    uint8_t rxBuffer[MAX_MONITOR_BUFFER];
} MonitorThread;

typedef struct {
    int isTaskBeingTraced[MAX_TRACED_TASKS];    // Used to know which instances are being used to trace a user task
    pthread_mutex_t isTaskBeingTraced_mutex;    // Used to protect the list of task being traced

    pthread_t cleanUpTask_id;
    MonitorThread monitor[MAX_TRACED_TASKS];
} Supervisor;

int Supervisor_init(Supervisor *const me);
pid_t Supervisor_checkNewTaskToTrace(Supervisor *const me);

#endif // __TASK_TRACE_SUPERVISOR_H__