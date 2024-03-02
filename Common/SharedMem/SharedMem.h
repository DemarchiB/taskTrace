#ifndef __TASK_TRACE_SHARED_MEM_H__
#define __TASK_TRACE_SHARED_MEM_H__

#include "../Process/Process.h"

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h> // ssize_t

typedef struct {
    PID pid;    // Process that is being traced by this instance of the shared mem
    int fd;     // file descriptor related to this shared mem
} SharedMem;

int SharedMem_userInit(SharedMem *const me, PID pid);
int SharedMem_userDeinit(SharedMem *const me);
ssize_t SharedMem_userWrite(const SharedMem *const me, const uint8_t *const buffer, size_t size);

int SharedMem_supervisorInit(SharedMem *const me, PID pid);
int SharedMem_supervisorDeinit(SharedMem *const me);
ssize_t SharedMem_supervisorRead(const SharedMem *const me, uint8_t *const buffer, size_t maxSize);

const char* SharedMem_getDefaultPath(void);


#endif // __TASK_TRACE_SHARED_MEM_H__