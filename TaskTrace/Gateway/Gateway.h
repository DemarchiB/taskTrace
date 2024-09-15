#ifndef __TASK_TRACE_GATEWAY_H__
#define __TASK_TRACE_GATEWAY_H__

#include "Queue/Queue.h"
#include "../../Common/SharedMem/SharedMem.h"
#include <pthread.h>

#include <stdbool.h>

typedef struct {
    Queue queue;
    SharedMem sharedMem;
    pthread_t gatewayThread;

    Telegram telegram;   // Put here to avoid using stack and then get a page fault
} Gateway;

int Gateway_init(Gateway *const me, pid_t pid);

int Gateway_write(Gateway *const me, const Telegram *const telegram);
int Gateway_update(Gateway *const me);


#endif // __TASK_TRACE_GATEWAY_H__