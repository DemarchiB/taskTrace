#ifndef __TASK_TRACE_RELEASE_JITTER_COMPENSATION_H__
#define __TASK_TRACE_RELEASE_JITTER_COMPENSATION_H__

#include <stdint.h>
#include <sys/types.h>

typedef struct {
    pid_t pid;
    uint64_t estimatedReleaseJitter;
    uint64_t teoricalPeriodRelease;
    const uint64_t deadlineInNs;    // equals runtime
    const uint64_t periodInNs;
} ReleaseJitterCompensation;

pid_t ReleaseJitterCompensation_getTaskPID(void);
uint64_t ReleaseJitterCompensation_getValueToCompensate(void);

void *ReleaseJitterCompensation_task(void *arg);
void *ReleaseJitterCompensation_monitorTask(void *arg);

#endif // __TASK_TRACE_RELEASE_JITTER_COMPENSATION_H__