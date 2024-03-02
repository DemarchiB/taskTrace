#ifndef __TASK_TRACE_PROCESS_H__
#define __TASK_TRACE_PROCESS_H__

// typedef struct {
//     int number; // The process ID number
// } PID;

typedef int PID;

PID PID_get(void);
int PID_checkIfExist(PID pid);


#endif // __TASK_TRACE_PROCESS_H__