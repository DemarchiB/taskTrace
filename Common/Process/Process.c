#include "Process.h"

#include <unistd.h> // getpid, getuid
#include <signal.h> // kill

#include <sys/syscall.h>    // syscall

/**
 * @brief Return the curent process' PID
 * 
 * @return PID 
 */
pid_t PID_get(void)
{
    return syscall(SYS_gettid);
}

/**
 * @brief Check if there is a process with the PID.
 * 
 * @param pid Process' ID
 * @return int  0 Doesn't exist
 *              1 Exist
 *              -1 Permission error. This function must be called by a super user
 */
int PID_checkIfExist(pid_t pid)
{
    if (geteuid() != 0) {
        // printf("I'm not with root permissions.\n");
        return -1;
    }

    // Trying to send a 0 signal. Accordingly to kill() manual:
    //      If sig is 0, then no signal is sent, but existence and permission
    //      checks are still performed; this can be used to check for the
    //      existence of a process ID or process group ID that the caller is
    //      permitted to signal.
    if (kill(pid, 0) == 0) {
        // printf("The process with the pid_t %d exist.\n", pid);
        return 1;
    } else {
        // printf("The process with the pid_t %d doesn't exist or you don't have permissions to access it.\n", pid);
        return 0;
    }
}
