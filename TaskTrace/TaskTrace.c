#include <stdio.h>

#include "TaskTrace.h"

#include <string.h>
#include <sys/mman.h>   // mlockall


static inline int TaskTrace_sendTelegram(TaskTrace *const me)
{
    // Read the actual time
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &me->telegram.timestamp) != 0) {
        perror("Error reding actual time to generate timestamp");
        return -1;
    }

    ssize_t ret = SharedMem_userWrite(&me->SharedMem, &me->telegram);

    if (ret != sizeof(Telegram)) {
        return -2;
    }

    return 0;
}

int TaskTrace_init(TaskTrace *const me)
{
    // config stack em heap to stay always allocated and avoid page faults during operation
    mlockall(MCL_CURRENT | MCL_FUTURE);

    memset(me, 0, sizeof(TaskTrace));

    me->pid = PID_get();

    if (SharedMem_userInit(&me->SharedMem, me->pid)) {
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

int TaskTrace_deadlineTaskStartPoint(TaskTrace *const me)
{
    if (!me->isInitiallized || !me->isRecording) {
        return -1;
    }

    me->telegram.pid = me->pid;
    me->telegram.code = TelegramCode_cyclicTaskFirstReady;

    return TaskTrace_sendTelegram(me);
}

int TaskTrace_traceWorkStart(TaskTrace *const me)
{
    if (!me->isInitiallized || !me->isRecording) {
        return -1;
    }

    me->telegram.pid = me->pid;
    me->telegram.code = TelegramCode_startWork;

    // Read the actual time
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &me->telegram.timestamp) != 0) {
        perror("Error reding actual time to generate timestamp");
        return -1;
    }

    ssize_t ret = SharedMem_userWrite(&me->SharedMem, &me->telegram);

    if (ret != sizeof(Telegram)) {
        return -2;
    }

    return 0;
}

int TaskTrace_traceWorkStop(TaskTrace *const me)
{
    if (!me->isInitiallized || !me->isRecording) {
        return -1;
    }

    me->telegram.pid = me->pid;
    me->telegram.code = TelegramCode_stopWork;

    // Read the actual time
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &me->telegram.timestamp) != 0) {
        perror("Error reding actual time to generate timestamp");
        return -1;
    }

    ssize_t ret = SharedMem_userWrite(&me->SharedMem, &me->telegram);

    if (ret != sizeof(Telegram)) {
        return -2;
    }

    return 0;
}
