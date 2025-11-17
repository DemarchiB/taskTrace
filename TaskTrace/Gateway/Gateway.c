#include "Gateway.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

static void *gateway_task(void *arg);

int Gateway_init(Gateway *const me, pid_t pid)
{
    Queue_init(&me->queue);

    if (SharedMem_userInit(&me->sharedMem, pid)) {
        return -1;
    }

    // Create a new thread
    if (pthread_create(&me->gatewayThread, NULL, gateway_task, me) != 0) {
        perror("Erro ao criar gateway");
        return -2;
    }

    return 0;
}

// Called by the deadline task to send data to the gateway
int Gateway_write(Gateway *const me, const Telegram *const telegram)
{
    if (Queue_add(&me->queue, telegram)) {
        return -1;
    }

    return 0;
}

// Used by the gateway to know when there are new data ready
int Gateway_update(Gateway *const me)
{
    if (Queue_getNext(&me->queue, &me->telegram)) {
        return -1;
    }

    ssize_t ret = SharedMem_userWrite(&me->sharedMem, &me->telegram);

    if (ret != sizeof(Telegram)) {
        if (errno != EAGAIN) {
            return -2;
        }

        // if here, the shared mem (FIFO) is full and we should remove the oldest data to
        // open space for the new data
        // TODO
    }

    return 0;
}

static void *gateway_task(void *arg)
{
    Gateway *const me = (Gateway *)arg;
    
    // Changing scheduler to SCHED_FIFO (RT)
    struct sched_param param = {30};
    if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
        printf("Gateway %d: Error changing scheduler to SCHED_FIFO: ", me->sharedMem.pid);
        perror("");
        return NULL;
    }
    
    printf("Gateway: Created new gateway for task %d\n", me->sharedMem.pid);

    while(1) {
        int ret = Gateway_update(me);

        if (ret) {
            printf("Gateway_update error %d\n", ret);
            perror("");
        }
    }

    return NULL;
}