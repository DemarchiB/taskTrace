#ifndef __TASK_TRACE_H__
#define __TASK_TRACE_H__

#include "../Common/SharedMem/SharedMem.h"

typedef struct {
    int isInitiallized;
    int isRecording;
    pid_t pid;
    SharedMem SharedMem;
    Telegram telegram;  // Used as txBuffer.

    struct timespec tmpTime;    // tmp value, not in stack to avoid page fault
} TaskTrace;

int TaskTrace_init(TaskTrace *const me);
int TaskTrace_deinit(TaskTrace *const me);

int TaskTrace_enableRecording(TaskTrace *const me);
int TaskTrace_disableRecording(TaskTrace *const me);

int TaskTrace_traceExecutionStart(TaskTrace *const me);
int TaskTrace_traceExecutionStop(TaskTrace *const me);

#endif // __TASK_TRACE_H__