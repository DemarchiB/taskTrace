#include "Supervisor.h"
#include "../Common/Process/Process.h"

#include <stdio.h>
#include <string.h>
#include <dirent.h>     // opendir, readdir, closedir
#include <stdlib.h>     // atoi
#include <sys/mman.h>   // mlockall
#include <unistd.h>     // usleep, getuid

#include <ncurses.h>    //  Interface

#ifndef SCHED_DEADLINE
#define SCHED_DEADLINE  6
#endif

static int Supervisor_init(Supervisor *const me, const UserInputs *const userInputs);
static pid_t Supervisor_checkNewTaskToTrace(Supervisor *const me);
static int Supervisor_reserveInstance(Supervisor *const me);
static int Supervisor_freeInstance(Supervisor *const me, int instanceNumber);
static int Supervisor_checkIfTaskIsBeingTraced(Supervisor *const me, pid_t pid);
static void *Supervisor_interfaceUpdateTask(void *arg);
static void *Supervisor_checkAndCleanUnusedSharedMemThread(void *arg);

static void Monitor_init(MonitorThread *const me);
static void Monitor_initMetrics(MonitorThread *const me);
static void *Monitor_task(void *arg);

static int UserInputs_read(UserInputs *const me, int argc, char *argv[]);

int main(int argc, char *argv[]) 
{
    static Supervisor supervisor;
    UserInputs userInputs;

    if (geteuid() != 0) {
        printf("Supervisor: Error, permission denied. Run as sudo.\n");
        return -1;
    }

    if (UserInputs_read(&userInputs, argc, argv)) {
        return -1;
    }

    printf("Supervisor: Starting configuration\n");
    if (Supervisor_init(&supervisor, &userInputs)) {
        printf("Supervisor: Error initiallizing\n");
        return -2;
    }

    printf("Supervisor: Creating cleanup task\n");
    if (pthread_create(&supervisor.cleanUpTask_id, NULL, Supervisor_checkAndCleanUnusedSharedMemThread, &supervisor) != 0) {
        perror("Supervisor: Error creating cleanup task");
        return -3;
    }

    printf("Supervisor: Creating interface update task\n");
    if (pthread_create(&supervisor.interfaceUpdateTask_id, NULL, Supervisor_interfaceUpdateTask, &supervisor) != 0) {
        perror("Supervisor: Error creating interface update task");
        return -4;
    }

    sleep(1);   // Wait for the cleanup task to make the first clean.

    while(1) {
        //printf("Supervisor: Checking for new tasks to trace\n");
        pid_t pid = Supervisor_checkNewTaskToTrace(&supervisor);
        //printf("Supervisor: new task to trace found with pid_t %d\n", (int) pid);

        int instanceNumber;
        //printf("Supervisor: checking for a free monitoring instace\n");
        while ((instanceNumber = Supervisor_reserveInstance(&supervisor)) < 0) {
            usleep(500000);
        }
       // printf("Supervisor: Free instance %d found\n", instanceNumber);

        supervisor.monitor[instanceNumber].pid = pid;

        //printf("Supervisor: Creating a thread to monitor the user process %d\n", pid);
        if (pthread_create(&supervisor.monitor[instanceNumber].id, 
                NULL, 
                Monitor_task, 
                &supervisor.monitor[instanceNumber]) != 0) {
            perror("Supervisor: Error creating monitoring thread");
            return -5;
        }
    }

    return 0;
}

static int Supervisor_init(Supervisor *const me, const UserInputs *const userInputs)
{
    // config stack em heap to stay always allocated and avoid page faults during operation
    mlockall(MCL_CURRENT | MCL_FUTURE);

    memset(me, 0, sizeof(Supervisor));

    if (pthread_mutex_init(&me->isTaskBeingTraced_mutex, NULL) != 0) {
        return -1;
    }

    memcpy(&me->userInputs, userInputs, sizeof(UserInputs));

    return 0;
}

static pid_t Supervisor_checkNewTaskToTrace(Supervisor *const me)
{
    int newTaskToTraceFound = 0;
    pid_t pid;

    DIR *dir;

    // Open the default shared mem region to search on
    while((dir = opendir(SharedMem_getDefaultPath())) == NULL);

    do {
        struct dirent *entry;
        usleep(50000);  //50ms
        rewinddir(dir);

        // Check if there is any new region created by some task that wants to be traced
        while ((entry = readdir(dir)) != NULL) {
            //printf("Supervisor_checkNewTaskToTrace: entry->d_name |%s|, pid_t = %d\n", entry->d_name, atoi(entry->d_name));
            if ((pid = (pid_t) atoi(entry->d_name)) <= 0) {
                continue;
            }

            int taskAlreadyBeingTraced = Supervisor_checkIfTaskIsBeingTraced(me, pid);
            
            if (taskAlreadyBeingTraced == 0) {
                newTaskToTraceFound = 1;
                break;
            }
        }
    } while (newTaskToTraceFound == 0);

    // Close the default shared mem region
    closedir(dir);

    return pid;
}

static void *Supervisor_checkAndCleanUnusedSharedMemThread(void *arg)
{
    Supervisor *const me = arg;
    DIR *dir;

    printf("Supervisor: Cleanup task created\n");
    // Open the default shared mem region to search on
    while((dir = opendir(SharedMem_getDefaultPath())) == NULL);

    while (1) {
        struct dirent *entry;

        // Check if there is a region created that does not correspont to any process
        while ((entry = readdir(dir)) != NULL) {
            pid_t pid = (pid_t) atoi(entry->d_name);
            // printf("Cleanup: entry->d_name = %s, pid = %d\n", entry->d_name, pid);
            
            if (pid <= 0) {
                continue;
            }

            // Do not remove memories that as still being monitored, even if the user process doesn't exist anymore.
            if (Supervisor_checkIfTaskIsBeingTraced(me, pid)) {
                continue;
            }

            if (PID_checkIfExist(pid) == 0) {
                char path[50];
                SharedMem_getFinalPath(path, pid);
                if (remove(path)) {
                    perror("Supervisor: Cleanup removing error\n");
                }
            }
        }
        
        rewinddir(dir);

        sleep(10);
    }

    // Close the default shared mem region
    closedir(dir);

    pthread_exit(NULL);
}

static void Monitor_init(MonitorThread *const me)
{
    me->policy = PID_getSchedulerPolicy(me->pid, NULL);

    Monitor_initMetrics(me);
}

static void Monitor_initMetrics(MonitorThread *const me)
{
    memset(&me->metrics, 0, sizeof(MonitorMetrics));

    me->metrics.minLatency = (uint64_t) -1;
    me->metrics.minET = (uint64_t) -1;
}

/**
 * @brief Each instance of this task is responsible for monitoring one user process.
 * 
 * @param arg Pointer to the sharedMem to monitor.
 * @return void* 
 */
static void *Monitor_task(void *arg)
{
    MonitorThread *const me = arg;
    
    printf("Supervisor: Monitor task for pid %d created\n", me->pid);
    
    SharedMem_supervisorInit(&me->sharedMem, me->pid);

    Monitor_init(me);

    while(1) {
        ssize_t numberOfBytesReceived = SharedMem_supervisorRead(&me->sharedMem, &me->telegram);

        if (numberOfBytesReceived != sizeof(Telegram)) {
            if (numberOfBytesReceived == 0) {
                if (PID_checkIfExist(me->pid != 1)) {
                    // User task was closed
                    pthread_exit(NULL);
                }
            }
            printf("Monitor %d: invalid telegram, received %ld bytes (should be %ld)\n", me->pid, numberOfBytesReceived, sizeof(Telegram));
            continue;
        }

        if (me->pid != me->telegram.pid) {
            printf("Monitor %d: invalid telegram, pid %d received (should be %d)\n", me->pid, me->telegram.pid, me->pid);
            continue;
        }

        switch (me->telegram.code)
        {
        case TelegramCode_cyclicTaskFirstReady:
            if (me->metrics.lastCyclicTaskReadyTime == 0) {
                me->metrics.lastCyclicTaskReadyTime = me->telegram.t1;
            }
            break;
        case TelegramCode_startAndStopTime:
            me->metrics.lastStartExecutionTime = me->telegram.t1;
            me->metrics.lastStopExecutionTime = me->telegram.t2;

            // Calculate the execution time
            me->metrics.lastET = me->metrics.lastStopExecutionTime - me->metrics.lastStartExecutionTime;

            // Calculate the maximum and minimum execution time values
            if (me->metrics.lastET > me->metrics.WCET) {
                me->metrics.WCET = me->metrics.lastET;
            } else if (me->metrics.lastET < me->metrics.minET) {
                me->metrics.minET = me->metrics.lastET;
            }

            do {
                if (me->policy == SCHED_DEADLINE) {
                    // Read task deadline parameters
                    uint64_t runtime, period, deadline;
                    if (PID_getDeadlinePropeties(me->pid, &runtime, &deadline, &period) != 0) {
                        me->policy = PID_getSchedulerPolicy(me->pid, NULL);
                        if (me->policy != SCHED_DEADLINE) {
                            printf("Monitor %d: Error, not able to read deadline properties cause user changed the policy to %d\n", me->pid, me->policy);
                        } else {
                            printf("Monitor %d: Error, not able to read deadline properties\n", me->pid);
                        }
                        break;
                    }

                    /*
                        Calculate latency if user provided the first trace tick
                        If user didn't call the trace for DeadlineTaskStartPoint, then I try to estimate using the trace point of the start exect time.
                        This might work we cause I also have a autoadjust, that will stay tuning this value forever.
                    */
                    if (me->metrics.lastCyclicTaskReadyTime == 0) {
                        me->metrics.lastCyclicTaskReadyTime = me->metrics.lastStartExecutionTime;
                    }

                    // Autoajust the ready tick in case the first tick was not 100% correct
                    if (me->metrics.lastStartExecutionTime < me->metrics.lastCyclicTaskReadyTime) {
                        //me->metrics.lastCyclicTaskReadyTime = me->metrics.lastStartExecutionTime;
                        me->metrics.lastStartExecutionTime = me->metrics.lastCyclicTaskReadyTime;
                    }

                    // Calculate latency
                    me->metrics.lastLatency = (int64_t) me->metrics.lastStartExecutionTime - me->metrics.lastCyclicTaskReadyTime;

                    if (me->metrics.lastLatency > me->metrics.maxLatency) {
                        me->metrics.maxLatency = me->metrics.lastLatency;
                    } else if (me->metrics.lastLatency < me->metrics.minLatency) {
                        me->metrics.minLatency = me->metrics.lastLatency;
                    }

                    // Check for deadline lost
                    if ((me->metrics.lastStopExecutionTime - me->metrics.lastCyclicTaskReadyTime) > deadline) {
                        me->metrics.deadlineLostCount++;
                    }
                    // if (me->metrics.lastET + me->metrics.lastLatency > deadline) {
                    //     me->metrics.deadlineLostCount++;
                    // }

                    // Check for task runtime overruns
                    if (me->metrics.lastET > runtime) {
                        me->metrics.runtimeOverrunCount++;

                        // Check for task "throttled" or "depleted" in case of overruns
                        if (me->metrics.lastET > period) {
                            me->metrics.taskDepletedCount++;
                        }
                    }
                    
                    // Update next tick where the task will be ready
                    // Here I have a while cause there could have happend some runtime overflows
                    while(me->metrics.lastCyclicTaskReadyTime < me->metrics.lastStopExecutionTime) {
                        me->metrics.lastCyclicTaskReadyTime += period;
                    }
                }
            } while(0);

            break;
        case TelegramCode_perfMark1Start:
            me->metrics.perfMarkStart[TelegramCode_perfMark1Start - TELEGRAM_PERFMARK_OFFSET] = me->telegram.t1;
            break;
        case TelegramCode_perfMark1End:
            me->metrics.perfMarkStop[TelegramCode_perfMark1End - TELEGRAM_PERFMARK_OFFSET] = me->telegram.t1;
            me->metrics.perfMartTotalTime[TelegramCode_perfMark1End - TELEGRAM_PERFMARK_OFFSET - 1] = 
                me->metrics.perfMarkStop[TelegramCode_perfMark1End - TELEGRAM_PERFMARK_OFFSET - 1] - 
                me->metrics.perfMarkStart[TelegramCode_perfMark1End - TELEGRAM_PERFMARK_OFFSET - 1];
            // printf("Monitor %d: performance mark %d work time = %ld ns\n", me->pid, TelegramCode_perfMark1Start - TELEGRAM_PERFMARK_OFFSET + 1, me->metrics.lastExecutionTime);
            break;
        default:
            // printf("Monitor %d: Error, invalid code (%d) received\n", me->pid, me->telegram.code);
            break;
        }

        // printf("Monitor %d: Telegram received successfully: \n", me->pid);
        // printf("       pid:      %d\n", me->telegram[0].pid);
        // printf("       code:     %d\n", me->telegram[0].code);
        // printf("       time(ns): %ld\n", me->lastStartExecutionTime);
    }

    pthread_exit(NULL);
}

static void *Supervisor_interfaceUpdateTask(void *arg)
{
    Supervisor *const me = arg;

    printf("Supervisor: Interface updater task created\n");

    sleep(3);

    // Inicializar o modo ncurses
    initscr();
    cbreak(); // Desabilitar buffering de linha
    noecho(); // Não exibir caracteres digitados
    curs_set(0); // Ocultar cursor

    // Duplicar a janela padrão para uma nova janela modificável
    // WINDOW *my_win = dupwin(stdscr);
    // if (my_win == NULL) {
    //     endwin();
    //     fprintf(stderr, "Erro ao duplicar a janela padrão\n");
    //     pthread_exit(NULL);
    // }

    char schedulerPolicy[20];
    int currentLine;
    uint64_t runtime, period, deadline;

    while (1) {
        clear(); // Limpar a tela

        currentLine = 0;
        mvprintw(currentLine++, 0, "%-20s %-1s %-1ld %-10s %-1s %-1d",
            "User inputs->",
            "sys_latency:",
            me->userInputs.systemLatency,
            "us",
            "autotune:",
            me->userInputs.autotune.isEnabled
            );

        // 1º Mostra tarefas do tipo deadline
        mvprintw(currentLine++, 0, "SCHED_DEADLINE TASKS:");
        mvprintw(currentLine++, 0, "PID      PRI    EXECTIME(ms)    WCET(ms)    Latency(us)   maxLat(us)  dlLosts  rtOverrun  depleted   RUNTIME(ms)    DEADLINE(ms)    PERIOD(ms)    RUN_USAGE%%    PROC_USAGE%%");

        pthread_mutex_lock(&me->isTaskBeingTraced_mutex);
        for (ssize_t i = 0; i < MAX_TRACED_TASKS; i++) {
            if (me->isTaskBeingTraced[i]) {
                int policy = PID_getSchedulerPolicy(me->monitor[i].pid, NULL);
                if (policy != SCHED_DEADLINE) { // 6 = sched_deadline
                    continue;
                }

                if (PID_getDeadlinePropeties(me->monitor[i].pid, &runtime, &deadline, &period) != 0) {
                    runtime = 0;
                    deadline = 0;
                    period = 0;
                }

                // Printa dados das tarefas
                mvprintw(currentLine++, 0, "%-8d %-6s %-15.3f %-11.3f %-13.1f %-11.1f %-8d %-10d %-10d %-14.3f %-15.3f %-13.3f %-13.1f %-3.1f",
                    me->monitor[i].pid,
                    "rt",
                    (float) (me->monitor[i].metrics.lastET / (1000 * 1000.0)),
                    (float) (me->monitor[i].metrics.WCET / (1000 * 1000.0)),
                    (float) (me->monitor[i].metrics.lastLatency / (1000.0)),
                    (float) (me->monitor[i].metrics.maxLatency / (1000.0)),
                    (uint32_t) (me->monitor[i].metrics.deadlineLostCount),
                    (uint32_t) (me->monitor[i].metrics.runtimeOverrunCount),
                    (uint32_t) (me->monitor[i].metrics.taskDepletedCount),
                    (float) (runtime / (1000 * 1000.0)),
                    (float) (deadline / (1000 * 1000.0)),
                    (float) (period / (1000 * 1000.0)),
                    (float) (me->monitor[i].metrics.WCET * 100.0 / runtime),
                    (float) (me->monitor[i].metrics.WCET * 100.0 / runtime) * ( (float) runtime / (float) period)
                    );
            }
        }
        pthread_mutex_unlock(&me->isTaskBeingTraced_mutex);

        mvprintw(currentLine++, 0, "\n"); // Newline

        // 2º Mostra tarefas do tipo FIFO
        mvprintw(currentLine++, 0, "SCHED_FIFO TASKS:");

        // Imprimir cabeçalho
        mvprintw(currentLine++, 0, "PID      PRI    EXECTIME(ms)    MIN_ET(ms)  WCET(ms)");

        pthread_mutex_lock(&me->isTaskBeingTraced_mutex);
        for (ssize_t i = 0; i < MAX_TRACED_TASKS; i++) {
            if (me->isTaskBeingTraced[i]) {
                int policy = PID_getSchedulerPolicy(me->monitor[i].pid, NULL);
                if (policy != SCHED_FIFO) {
                    continue;
                }

                // Printa dados das tarefas
                mvprintw(currentLine++, 0, "%-8d %-6d %-15.3f %-11.3f %-13.3f",
                    me->monitor[i].pid,
                    -1 - PID_getPriority(me->monitor[i].pid),
                    (float) (me->monitor[i].metrics.lastET / (1000 * 1000.0)),
                    (float) (me->monitor[i].metrics.minET / (1000 * 1000.0)),
                    (float) (me->monitor[i].metrics.WCET / (1000 * 1000.0))
                    );
            }
        }
        pthread_mutex_unlock(&me->isTaskBeingTraced_mutex);
        
        mvprintw(currentLine++, 0, "\n"); // Newline

        // 3º Mostra todos os outros tipos de tarefas
        mvprintw(currentLine++, 0, "Other policys:");

        // Imprimir cabeçalho
        mvprintw(currentLine++, 0, "PID      SCHEDTYPE       PRI    EXECTIME(ms)    MIN_ET(ms)  WCET(ms)");

        pthread_mutex_lock(&me->isTaskBeingTraced_mutex);
        for (ssize_t i = 0; i < MAX_TRACED_TASKS; i++) {
            if (me->isTaskBeingTraced[i]) {
                int policy = PID_getSchedulerPolicy(me->monitor[i].pid, schedulerPolicy);
                if ((policy == SCHED_FIFO) || (policy == 6)) {
                    continue;
                }

                if (policy < 0) {
                    snprintf(schedulerPolicy, sizeof(schedulerPolicy), "UNKNOWN");
                }

                // Printa dados das tarefas
                mvprintw(currentLine++, 0, "%-8d %-15s %-6d %-15.3f %-11.3f %-13.3f",
                    me->monitor[i].pid,
                    schedulerPolicy,
                    PID_getPriority(me->monitor[i].pid),
                    (float) (me->monitor[i].metrics.lastET / (1000 * 1000.0)),
                    (float) (me->monitor[i].metrics.minET / (1000 * 1000.0)),
                    (float) (me->monitor[i].metrics.WCET / (1000 * 1000.0))
                    );
            }
        }
        pthread_mutex_unlock(&me->isTaskBeingTraced_mutex);

        refresh(); // Atualizar a tela

        // Aguardar um segundo
        sleep(1);
    }

    pthread_exit(NULL);
}

/**
 * @brief Reserve the first free instance found
 * 
 * @param me 
 * @return int The instance number. -1 if no free instance.
 */
static int Supervisor_reserveInstance(Supervisor *const me)
{
    int instanceNumber;
    int freeInstanceFound = 0;

    pthread_mutex_lock(&me->isTaskBeingTraced_mutex);
    for (instanceNumber = 0; instanceNumber < MAX_TRACED_TASKS; instanceNumber++) {
        if (me->isTaskBeingTraced[instanceNumber] == 0) {
            me->isTaskBeingTraced[instanceNumber] = 1;
            freeInstanceFound = 1;
            break;
        }
    }
    pthread_mutex_unlock(&me->isTaskBeingTraced_mutex);

    if (freeInstanceFound) {
        return instanceNumber;
    }

    return -1;  // No free instance found
}

static int Supervisor_freeInstance(Supervisor *const me, int instanceNumber)
{
    pthread_mutex_lock(&me->isTaskBeingTraced_mutex);
    if (me->isTaskBeingTraced[instanceNumber] == 0) {
        pthread_mutex_unlock(&me->isTaskBeingTraced_mutex);
        return -1;  //instance not being used, we should not call a free() to a already freed instance
    }

    me->isTaskBeingTraced[instanceNumber] = 0;
    pthread_mutex_unlock(&me->isTaskBeingTraced_mutex);

    return 0;
}

/**
 * @brief Check if the task is already being traced
 * 
 * @param me 
 * @param pid the pid that you want to check if is already being traced
 * @return int 
 */
static int Supervisor_checkIfTaskIsBeingTraced(Supervisor *const me, pid_t pid)
{
    int isTaskBeingTraced = 0;
    pthread_mutex_lock(&me->isTaskBeingTraced_mutex);
    for (int i = 0; i < MAX_TRACED_TASKS; i++) {
        if (me->isTaskBeingTraced[i] && (me->monitor[i].pid == pid)) {
            isTaskBeingTraced = 1;
            break;
        }
    }
    pthread_mutex_unlock(&me->isTaskBeingTraced_mutex);

    return isTaskBeingTraced;
}

static int UserInputs_read(UserInputs *const me, int argc, char *argv[])
{
    memset(me, 0, sizeof(UserInputs));

    int sys_latency_received = 0;

    if (argc < 2) {
        printf("Supervisor: Error, user must pass at least the sys_latency value.\n");
        printf("Usage: sudo Supervisor --sys_latency 500\n");
        printf("   or: sudo Supervisor --sys_latency 500 --autotune on\n");
        printf("Obs: The latency must be in microseconds (us)\n");
        return -1;
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--sys_latency") == 0) {
            // Check if there is a value after "--sys_latency"
            if (i + 1 < argc) {
                // Convert the value to systemLatency
                me->systemLatency = atoi(argv[i + 1]);
                i++; // jump to the next argument.
                sys_latency_received = 1;
            } else {
                printf("Supervisor: missing value for \"--sys_latency\"\n");
                return -1;
            }
        }
        else if (strcmp(argv[i], "--autotune") == 0) {
            // Check if there is a value after "--autotune"
            if (i + 1 < argc) {
                if (strcmp(argv[i + 1], "on") == 0) {
                    me->autotune.isEnabled = 1;
                } else if (strcmp(argv[i + 1], "off") == 0) {
                    me->autotune.isEnabled = 0;
                } else {
                    printf("Supervisor: Invalid value for \"--autotune\"\n");
                    printf("Supervisor: Valid values \"on\" and \"off\"\n");
                    return -2;
                }
                i++; // jump to the next argument.
            } else {
                printf("Supervisor: missing value for \"--autotune\"\n");
                return -2;
            }
        }
        else if ((strcmp(argv[i], "-h") == 0) || (strcmp(argv[i], "--help") == 0)) {
            printf("Usage: sudo Supervisor --sys_latency 500\n");
            printf("   or: sudo Supervisor --sys_latency 500 --autotune on\n");
            printf("   or: sudo Supervisor --sys_latency 500 --autotune off\n");
            printf("The system maximum latency (in us) is a mandatory argument\n");
            printf("You should get this value using other tools, like RTLA.\n");
            return -3; // Return error just to avoid the supervisor to move on
        }
    }

    if (sys_latency_received == 0) {
        printf("Supervisor: Error, user must pass at least the sys_latency value.\n");
        printf("Usage: sudo Supervisor --sys_latency 500\n");
        printf("   or: sudo Supervisor --sys_latency 500 --autotune on\n");
        printf("Obs: The latency must be in microseconds (us)\n");
        return -1;
    }


    return 0;
}