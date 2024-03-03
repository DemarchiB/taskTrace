#ifndef __TASK_TRACE_PROCESS_H__
#define __TASK_TRACE_PROCESS_H__

#include <sys/types.h>

pid_t PID_get(void);
int PID_getSchedulerPolicy(pid_t pid, char *const policyAsString);
int PID_getPriority(pid_t pid);
int PID_checkIfExist(pid_t pid);

#endif // __TASK_TRACE_PROCESS_H__