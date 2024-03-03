#ifndef __TASK_TRACE_PROCESS_H__
#define __TASK_TRACE_PROCESS_H__

#include <sys/types.h>
#include <stdint.h>

pid_t PID_get(void);
int PID_getSchedulerPolicy(pid_t pid, char *const policyAsString);
int PID_getPriority(pid_t pid);
int PID_checkIfExist(pid_t pid);

int PID_getDeadlinePropeties(pid_t pid, 
                            uint64_t *const runtime, 
                            uint64_t *const deadline, 
                            uint64_t *const period);
int PID_setDeadline(pid_t pid, uint64_t runtime, uint64_t deadline, uint64_t period);  // just for internal tests, is not needed neither by the supervisor nor by the user lib.

#endif // __TASK_TRACE_PROCESS_H__