#ifndef __TASK_TRACE_SUPERVISOR_H__
#define __TASK_TRACE_SUPERVISOR_H__

#include "../Common/SharedMem/SharedMem.h"
#include "Monitor/Monitor.h"
#include "UserInputs/UserInputs.h"

#include <stdbool.h>

#define MAX_TRACED_TASKS 100

typedef struct {
    UserInputs userInputs;
    int isTaskBeingTraced[MAX_TRACED_TASKS];    // Used to know which instances are being used to trace a user task
    pthread_mutex_t isTaskBeingTraced_mutex;    // Used to protect the list of task being traced

    pthread_t checkTask_id;
    pthread_t cleanUpTask_id;
    MonitorThread monitor[MAX_TRACED_TASKS];
} Supervisor;

int Supervisor_init(Supervisor *const me, const UserInputs *const userInputs);

#endif // __TASK_TRACE_SUPERVISOR_H__