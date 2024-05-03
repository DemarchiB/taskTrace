#ifndef __TASK_TRACE_H__
#define __TASK_TRACE_H__

#include "../Common/SharedMem/SharedMem.h"

typedef struct {
    int isInitiallized;
    int isRecording;
    pid_t pid;
    SharedMem SharedMem;
    Telegram telegram;  // Used as txBuffer.
} TaskTrace;

int TaskTrace_init(TaskTrace *const me);
int TaskTrace_deinit(TaskTrace *const me);

int TaskTrace_enableRecording(TaskTrace *const me);
int TaskTrace_disableRecording(TaskTrace *const me);

int TaskTrace_deadlineTaskStartPoint(TaskTrace *const me);

int TaskTrace_traceWorkStart(TaskTrace *const me);
int TaskTrace_traceWorkStop(TaskTrace *const me);

#endif // __TASK_TRACE_H__