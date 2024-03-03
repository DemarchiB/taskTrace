#include "Process.h"

#ifndef SCHED_DEADLINE
#define SCHED_DEADLINE 6
#endif

#include <stdio.h>  // snprintf
#include <unistd.h> // getpid, getuid
#include <signal.h> // kill
#include <sched.h>  // sched_getparam, sched_getscheduler

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
 * @brief Read the scheduling policy for a specified process
 * 
 * @param pid The pid of the process that you want to read
 * @param policyAsString If not null, the policy type will be printed here as string (minimum 16 chars, otherwise there will be memory access errors)
 * @return int the Policy number
 */
int PID_getSchedulerPolicy(pid_t pid, char *const policyAsString)
{ 
    int policy;
    
    // Read the scheduling policy
    policy = sched_getscheduler(pid);
    if (policy == -1) {
        return -1;
    }

    if (policyAsString == NULL) {
        return policy;
    }

    // Prinf informations at the 
    switch(policy) {
        case SCHED_OTHER:
            snprintf(policyAsString, 16, "SCHED_OTHER");
            break;
        case SCHED_FIFO:
            snprintf(policyAsString, 16, "SCHED_FIFO");
            break;
        case SCHED_DEADLINE:
            snprintf(policyAsString, 16, "SCHED_DEADLINE");
            break;
        case SCHED_RR:
            snprintf(policyAsString, 16, "SCHED_RR");
            break;
        default:
            snprintf(policyAsString, 16, "UNKNOWN");
    }

    return policy;
}

int PID_getPriority(pid_t pid)
{
    // Structure to save the scheduler params
    struct sched_param param;

    // Read scheduler informations
    if (sched_getparam(pid, &param) == -1) {
        return -1;
    }

    return param.sched_priority;
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
