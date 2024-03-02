
#include "SharedMem.h"

#include <stdio.h>      // snprintf
#include <string.h>

#include <unistd.h>     // access, close, read
#include <fcntl.h>      // open
#include <dirent.h>     // opendir, readdir
#include <sys/types.h>  // mkfifo
#include <fcntl.h>      // mkfifoat
#include <sys/stat.h>   // mkdir, mkfifo, mkfifoat

static const char middlePath[] = "/tmp/udesc";  // should not be needed, but I was lasy to do some logic to avoid it
static const char defaultPath[] = "/tmp/udesc/taskMonitor/";

/**
 * @brief Must be called to create the shared memory region from the user app.
 * 
 * @param me Pointer to the shared memory instance responsible for the pid_t
 * @param pid The current user task pid_t
 * @return int 0 if ok
 *              != 0 if error
 */
int SharedMem_userInit(SharedMem *const me, pid_t pid)
{
    char sharedMem_Path[sizeof(defaultPath) + 20];

    me->pid = pid;

    // 1º Convert the pid_t to string and generate the final path
    SharedMem_getFinalPath(sharedMem_Path, me->pid);

    // 2º Create the shared memory region
    // 2.1 Check if there is no unused mem where we will create our
    if (access(sharedMem_Path, F_OK) == 0) {
        // If the region exist, return error. 
        // The supervisor should remove unused regions, so, if the region is there, than its probably being used
        return -1;
    }

    // 2.2 Create the new shared mem
    mkdir(middlePath, 0777);    // Should not be need, but I'm lasy sometimes
    mkdir(defaultPath, 0777);    // Should not be need, but I'm lasy sometimes

    // 2.3 Create the named pipe with r/w permission for all kind of users
    // if (mkfifo(sharedMem_Path, 0666) != 0) {
    if (mkfifoat(0, sharedMem_Path, 0666) != 0) {
        perror("SharedMem_userInit: Error to create the shared mem region");
        return -2;
    }

    // 2.4 Open the fifo as write only, since we will only send data through
    // CAREFULL: THIS CALL WILL BLOCK UNTIL THE SUPERVISOR OPENS IT AS READ ONLY.
    if ((me->fd = open(sharedMem_Path, O_RDWR | O_NONBLOCK)) == -1) {
        perror("SharedMem_userInit: Error opening the shared mem region");
        printf("Tried to open %s\n", sharedMem_Path);
        return -3;
    }

    // Now, we just need to write to the fd and the supervisor will receive.
    return 0;
}

/**
 * @brief Must be called by the supervisor to register/shared the memory region created by the user
 *          Carefull, this function has no timeout.
 * 
 * @param me Pointer to the shared memory instance responsible for the pid_t
 * @param pid The user task pid_t that will be monitored.
 * @return int 0 if ok
 *              != 0 if error
 */
int SharedMem_supervisorInit(SharedMem *const me, pid_t pid)
{
    char sharedMem_Path[sizeof(defaultPath) + 20];

    me->pid = pid;

    // 1º Convert the pid_t to string and generate the final path
    SharedMem_getFinalPath(sharedMem_Path, me->pid);

    // 2º Check/wait for the named pipe creation infinitelly
    while (access(sharedMem_Path, F_OK) != 0);

    // 3º Open the fifo as read only, since we will only read the data sent by the user task
    // CAREFULL: THIS CALL WILL BLOCK UNTIL THE SUPERVISOR OPENS IT AS WRITE ONLY.
    if ((me->fd = open(sharedMem_Path, O_RDONLY)) == -1) {
        perror("SharedMem_supervisorInit: Error opening the shared mem region");
        printf("Tried to open %s\n", sharedMem_Path);
        return -1;
    }

    return 0;
}

int SharedMem_userDeinit(SharedMem *const me)
{
    char sharedMem_Path[sizeof(defaultPath) + 20];

    // 1º Convert the pid_t to string and generate the final path
    SharedMem_getFinalPath(sharedMem_Path, me->pid);

    // 2º Close and try to delete the named pipe (may return error if the supervisor still has it opened)
    close(me->fd);
    unlink(sharedMem_Path);

    return 0;
}

int SharedMem_supervisorDeinit(SharedMem *const me)
{
    char sharedMem_Path[sizeof(defaultPath) + 20];

    // 1º Convert the pid_t to string and generate the final path
    SharedMem_getFinalPath(sharedMem_Path, me->pid);

    // 2º Close and try to delete the named pipe (may return error if the user task still has it opened)
    close(me->fd);
    unlink(sharedMem_Path);

    return 0;
}

ssize_t SharedMem_userWrite(const SharedMem *const me, const Telegram *const telegram)
{
    return write(me->fd, telegram, sizeof(Telegram));
}

ssize_t SharedMem_supervisorRead(const SharedMem *const me, Telegram *const telegram)
{
    return read(me->fd, telegram, sizeof(Telegram));
}

const char* SharedMem_getDefaultPath(void)
{
    return defaultPath;
}

/**
 * @brief Convert the pid_t to string and generate the final named pipe path
 * 
 * @param finalPath 
 * @param pid 
 */
void SharedMem_getFinalPath(char *const finalPath, pid_t pid)
{
    char pidAsString[20];

    // 1º Convert the pid_t to string and generate the final path
    snprintf(pidAsString, sizeof(pidAsString), "%d", pid);
    memcpy(finalPath, defaultPath, sizeof(defaultPath));
    strcat(finalPath, pidAsString);

    return;
}