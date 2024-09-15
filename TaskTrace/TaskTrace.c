#include <stdio.h>

#include "TaskTrace.h"

#include <string.h>
#include <sys/mman.h>   // mlockall

static inline int TaskTrace_readTimestamp(struct timespec *timestamp)
{
    if (clock_gettime(CLOCK_MONOTONIC_RAW, timestamp) != 0) {
        // perror("Error reding actual time to generate timestamp");
        return -1;
    }

    return 0;
}

// Must be called before changing the task to deadline.
int TaskTrace_init(TaskTrace *const me)
{
    // config stack em heap to stay always allocated and avoid page faults during operation
    mlockall(MCL_CURRENT | MCL_FUTURE);

    memset(me, 0, sizeof(TaskTrace));

    me->pid = PID_get();

    if (Gateway_init(&me->gateway, me->pid)) {
        return -1;
    }

    me->isInitiallized = 1;

    return 0;
}

int TaskTrace_deinit(TaskTrace *const me)
{
    (void) me;
    return -1;
}

int TaskTrace_enableRecording(TaskTrace *const me)
{
    if (me->isInitiallized == 0) {
        return -1;
    }

    if (me->isRecording) {
        return -2;
    }

    me->isRecording = 1;

    return 0;
}

int TaskTrace_disableRecording(TaskTrace *const me)
{
    if (me->isInitiallized == 0) {
        return -1;
    }

    if (me->isRecording == 0) {
        return -2;
    }

    me->isRecording = 0;
    
    return 0;
}

int TaskTrace_traceExecutionStart(TaskTrace *const me)
{
    if (!me->isInitiallized || !me->isRecording) {
        return -1;
    }
    
    // Read the actual time
    if (TaskTrace_readTimestamp(&me->tmpTime)) {
        return -2;
    }
    
    me->telegram.t1 = (me->tmpTime.tv_sec * 1000 * 1000 * 1000) + (me->tmpTime.tv_nsec);

    return 0;
}

int TaskTrace_traceExecutionStop(TaskTrace *const me)
{
    if (!me->isInitiallized || !me->isRecording) {
        return -1;
    }

    me->telegram.pid = me->pid;
    me->telegram.code = TelegramCode_startAndStopTime;

    // Read the actual time
    if (TaskTrace_readTimestamp(&me->tmpTime)) {
        return -2;
    }
    
    me->telegram.t2 = (me->tmpTime.tv_sec * 1000 * 1000 * 1000) + (me->tmpTime.tv_nsec);

    if (Gateway_write(&me->gateway, &me->telegram)) {
        return -3;
    }

    return 0;
}
