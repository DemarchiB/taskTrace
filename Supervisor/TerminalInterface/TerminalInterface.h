#ifndef __TASK_TRACE_TERMINAL_INTERFACE_H__
#define __TASK_TRACE_TERMINAL_INTERFACE_H__

#include "../Supervisor.h"

typedef struct {
    const volatile Supervisor *supervisor;

    pthread_t interfaceUpdateTask_id;
} TerminalInterface;

int TerminalInterface_start(TerminalInterface *const me, const volatile Supervisor *const supervisor);

#endif // __TASK_TRACE_TERMINAL_INTERFACE_H__