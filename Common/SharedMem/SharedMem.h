#ifndef __TASK_TRACE_SHARED_MEM_H__
#define __TASK_TRACE_SHARED_MEM_H__

#include "../Process/Process.h"
#include "../Telegram/Telegram.h"

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h> // ssize_t

typedef struct {
    pid_t pid;    // Process that is being traced by this instance of the shared mem
    int fd;     // file descriptor related to this shared mem
} SharedMem;

int SharedMem_userInit(SharedMem *const me, pid_t pid);
int SharedMem_userDeinit(SharedMem *const me);
ssize_t SharedMem_userWrite(const SharedMem *const me, const Telegram *const telegram);

int SharedMem_supervisorInit(SharedMem *const me, pid_t pid);
int SharedMem_supervisorDeinit(SharedMem *const me);
ssize_t SharedMem_supervisorRead(const SharedMem *const me, Telegram *const telegram);

const char* SharedMem_getDefaultPath(void);
void SharedMem_getFinalPath(char *const finalPath, pid_t pid);


#endif // __TASK_TRACE_SHARED_MEM_H__