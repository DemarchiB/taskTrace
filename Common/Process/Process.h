#ifndef __TASK_TRACE_PROCESS_H__
#define __TASK_TRACE_PROCESS_H__

#include <sys/types.h>

pid_t PID_get(void);
int PID_checkIfExist(pid_t pid);

#endif // __TASK_TRACE_PROCESS_H__