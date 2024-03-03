#include "Process.h"

#ifndef SCHED_DEADLINE
#define SCHED_DEADLINE 6
#endif

#include <stdio.h>  // snprintf
#include <unistd.h> // getpid, getuid
#include <signal.h> // kill
#include <sched.h>  // sched_getparam, sched_getscheduler

#include <sys/syscall.h>    // syscall

#include <sched.h>            /* Definition of SCHED_* constants */
#include <sys/syscall.h>      /* Definition of SYS_* constants */
#include <unistd.h>

struct sched_attr {
    uint32_t size;              // Tamanho da estrutura em bytes
    uint32_t sched_policy;      // Política de escalonamento
    uint64_t sched_flags;       // Flags do escalonador (normalmente não é utilizado)
    int32_t sched_nice;         // Valor nice do processo
    uint32_t sched_priority;    // Prioridade do processo
    uint64_t sched_runtime;     // Tempo de execução em nanossegundos
    uint64_t sched_deadline;    // Prazo em nanossegundos
    uint64_t sched_period;      // Período em nanossegundos
};

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

/**
 * @brief 
 * 
 * @param pid process id
 * @param runtime output for the runtime value in ns
 * @param deadline output for the deadline value in ns
 * @param period output for the period value in ns
 * @return int 
 */
int PID_getDeadlinePropeties(pid_t pid, 
                            uint64_t *const runtime, 
                            uint64_t *const deadline, 
                            uint64_t *const period)
{
    if (PID_getSchedulerPolicy(pid, NULL) != SCHED_DEADLINE) {
        return -1;
    }

    struct sched_attr attr = {0};

    // Configure a política do escalonador
    if (syscall(SYS_sched_getattr, pid, &attr, sizeof(attr), 0) != 0) {
        return -2;
    }

    if (runtime != NULL) {
        *runtime = attr.sched_runtime;
    }

    if (deadline != NULL) {
        *deadline = attr.sched_deadline;
    }

    if (period != NULL) {
        *period = attr.sched_period;
    }

    return 0;
}

/**
 * @brief Change the scheduler policy of a process to SCHED_DEADLINE
 * 
 * @param pid process to change
 * @param runtime in ns
 * @param deadline in ns
 * @param period in ns
 * @return int 
 */
int PID_setDeadline(pid_t pid, uint64_t runtime, uint64_t deadline, uint64_t period)
{
    struct sched_attr attr = {0};

    attr.size = sizeof(attr); // sizeof(struct sched_attr)
    attr.sched_policy = SCHED_DEADLINE;
    attr.sched_runtime = runtime;
    attr.sched_deadline = deadline;
    attr.sched_period = period;

    // Configure a política do escalonador
    if (syscall(SYS_sched_setattr, pid, &attr, 0) != 0) {
        perror("Process: sched_setattr error.");
        return -1;
    }

    return 0;
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
