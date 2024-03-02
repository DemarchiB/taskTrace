#ifndef __TASK_TRACE_SUPERVISOR_H__
#define __TASK_TRACE_SUPERVISOR_H__

#include "../Common/SharedMem/SharedMem.h"

#include <pthread.h>

#define MAX_TRACED_TASKS 10

typedef struct {
    int isTaskBeingTraced[MAX_TRACED_TASKS];
    SharedMem sharedMem[MAX_TRACED_TASKS];

    pthread_t cleanUpTask_id;
    pthread_t supervisorTask_id[MAX_TRACED_TASKS];
} Supervisor;

int Supervisor_init(Supervisor *const me);
PID Supervisor_checkNewTaskToTrace(Supervisor *const me);

#endif // __TASK_TRACE_SUPERVISOR_H__