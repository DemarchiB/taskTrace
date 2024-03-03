#include "Supervisor.h"
#include "../Common/Process/Process.h"

#include <stdio.h>
#include <string.h>
#include <dirent.h>     // opendir, readdir, closedir
#include <stdlib.h>     // atoi
#include <sys/mman.h>   // mlockall
#include <unistd.h>     // usleep, getuid

#include <ncurses.h>    //  Interface

static void *Supervisor_checkAndCleanUnusedSharedMemThread(void *arg);
static void *Monitor_task(void *arg);
static void *Supervisor_interfaceUpdateTask(void *arg);

static int Supervisor_reserveInstance(Supervisor *const me);
static int Supervisor_freeInstance(Supervisor *const me, int instanceNumber);
static int Supervisor_checkIfTaskIsBeingTraced(Supervisor *const me, pid_t pid);

int main() 
{
    Supervisor supervisor;

    if (geteuid() != 0) {
        printf("Supervisor: Error, permission denied. Run as sudo.\n");
        return -1;
    }

    printf("Supervisor: Starting configuration\n");
    if (Supervisor_init(&supervisor)) {
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

int Supervisor_init(Supervisor *const me)
{
    // config stack em heap to stay always allocated and avoid page faults during operation
    mlockall(MCL_CURRENT | MCL_FUTURE);

    memset(me, 0, sizeof(Supervisor));

    if (pthread_mutex_init(&me->isTaskBeingTraced_mutex, NULL) != 0) {
        return -1;
    }

    // Adjust max and minumum values
    for (ssize_t i = 0; i < MAX_TRACED_TASKS; i++) {
        me->monitor[i].metrics.minWorkTime = (uint64_t) -1;
        me->monitor[i].metrics.maxWorkTime = 0;
    }

    return 0;
}

pid_t Supervisor_checkNewTaskToTrace(Supervisor *const me)
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
    
    // 1º Open the shared mem as read only
    SharedMem_supervisorInit(&me->sharedMem, me->pid);

    while(1) {
        ssize_t numberOfBytesReceived = SharedMem_supervisorRead(&me->sharedMem, &me->telegram);

        if (numberOfBytesReceived != sizeof(Telegram)) {
            printf("Monitor %d: invalid telegram, received %ld bytes (should be %ld)\n", me->pid, numberOfBytesReceived, sizeof(Telegram));
            continue;
        }

        if (me->pid != me->telegram.pid) {
            printf("Monitor %d: invalid telegram, pid %d received (should be %d)\n", me->pid, me->telegram.pid, me->pid);
            continue;
        }

        switch (me->telegram.code)
        {
        case TelegramCode_startWorkPoint:
            me->metrics.lastStartWorkTime = (me->telegram.timestamp.tv_sec * 1000 * 1000 * 1000) + (me->telegram.timestamp.tv_nsec);
            break;
        case TelegramCode_stopWorkPoint:
            me->metrics.lastStopWorkTime = (me->telegram.timestamp.tv_sec * 1000 * 1000 * 1000) + (me->telegram.timestamp.tv_nsec);

            // Calculate the work time
            me->metrics.lastWorkTime = me->metrics.lastStopWorkTime - me->metrics.lastStartWorkTime;

            // Calculate the maximum and minimum values
            if (me->metrics.lastWorkTime > me->metrics.maxWorkTime) {
                me->metrics.maxWorkTime = me->metrics.lastWorkTime;
            }
            
            if (me->metrics.lastWorkTime < me->metrics.minWorkTime) {
                me->metrics.minWorkTime = me->metrics.lastWorkTime;
            }

            // printf("Monitor %d: total work time = %ld ns\n", me->pid, me->metrics.lastWorkTime);
            break;
        case TelegramCode_perfMark1Start:
            me->metrics.perfMarkStart[TelegramCode_perfMark1Start - TELEGRAM_PERFMARK_OFFSET] = 
                (me->telegram.timestamp.tv_sec * 1000 * 1000 * 1000) + (me->telegram.timestamp.tv_nsec);
            break;
        case TelegramCode_perfMark1End:
            me->metrics.perfMarkStop[TelegramCode_perfMark1End - TELEGRAM_PERFMARK_OFFSET] = 
                (me->telegram.timestamp.tv_sec * 1000 * 1000 * 1000) + (me->telegram.timestamp.tv_nsec);
            me->metrics.perfMartTotalTime[TelegramCode_perfMark1End - TELEGRAM_PERFMARK_OFFSET - 1] = 
                me->metrics.perfMarkStop[TelegramCode_perfMark1End - TELEGRAM_PERFMARK_OFFSET - 1] - me->metrics.perfMarkStart[0];
            // printf("Monitor %d: performance mark %d work time = %ld ns\n", me->pid, TelegramCode_perfMark1Start - TELEGRAM_PERFMARK_OFFSET + 1, me->metrics.lastWorkTime);
            break;
        default:
            // printf("Monitor %d: Error, invalid code (%d) received\n", me->pid, me->telegram.code);
            break;
        }

        // printf("Monitor %d: Telegram received successfully: \n", me->pid);
        // printf("       pid:      %d\n", me->telegram[0].pid);
        // printf("       code:     %d\n", me->telegram[0].code);
        // printf("       time(ns): %ld\n", me->lastStartWorkTime);
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
    WINDOW *my_win = dupwin(stdscr);
    if (my_win == NULL) {
        endwin();
        fprintf(stderr, "Erro ao duplicar a janela padrão\n");
        pthread_exit(NULL);
    }

    char schedulerPolicy[20];
    int currentLine;
    uint64_t runtime, period, deadline;

    while (1) {
        clear(); // Limpar a tela

        currentLine = 0;

        // 1º Mostra tarefas do tipo deadline
        mvprintw(currentLine++, 0, "SCHED_DEADLINE TASKS: (all are PRI = RT)");
        mvprintw(currentLine++, 0, "PID      PRI    WORKTIME(ms)    MINTIME(ms)     MAXTIME(ms)    RUNTIME(ms)    DEADLINE(ms)    PERIOD(ms)    RUN_USAGE%%    PROC_USAGE%%");

        pthread_mutex_lock(&me->isTaskBeingTraced_mutex);
        for (ssize_t i = 0; i < MAX_TRACED_TASKS; i++) {
            if (me->isTaskBeingTraced[i]) {
                int policy = PID_getSchedulerPolicy(me->monitor[i].pid, NULL);
                if (policy != 6) { // 6 = sched_deadline
                    continue;
                }

                if (PID_getDeadlinePropeties(me->monitor[i].pid, &runtime, &deadline, &period) != 0) {
                    runtime = 0;
                    deadline = 0;
                    period = 0;
                }

                // Printa dados das tarefas
                mvprintw(currentLine++, 0, "%-8d %-6s %-16.3f %-14.3f %-14.3f %-14.3f %-15.3f %-13.3f %-13.1f %-3.1f",
                    me->monitor[i].pid,
                    "rt",
                    (float) (me->monitor[i].metrics.lastWorkTime / (1000 * 1000.0)),
                    (float) (me->monitor[i].metrics.minWorkTime / (1000 * 1000.0)),
                    (float) (me->monitor[i].metrics.maxWorkTime / (1000 * 1000.0)),
                    (float) (runtime / (1000 * 1000.0)),
                    (float) (deadline / (1000 * 1000.0)),
                    (float) (period / (1000 * 1000.0)),
                    (float) (me->monitor[i].metrics.maxWorkTime * 100.0 / runtime),
                    (float) (me->monitor[i].metrics.maxWorkTime * 100.0 / runtime) * ( (float) runtime / (float) period)
                    );
            }
        }
        pthread_mutex_unlock(&me->isTaskBeingTraced_mutex);

        mvprintw(currentLine++, 0, "\n"); // Newline

        // 2º Mostra tarefas do tipo FIFO
        mvprintw(currentLine++, 0, "SCHED_FIFO TASKS:");

        // Imprimir cabeçalho
        mvprintw(currentLine++, 0, "PID      PRI    WORKTIME(us)    MINTIME(us)     MAXTIME(us)");

        pthread_mutex_lock(&me->isTaskBeingTraced_mutex);
        for (ssize_t i = 0; i < MAX_TRACED_TASKS; i++) {
            if (me->isTaskBeingTraced[i]) {
                int policy = PID_getSchedulerPolicy(me->monitor[i].pid, NULL);
                if (policy != SCHED_FIFO) {
                    continue;
                }

                // Printa dados das tarefas
                mvprintw(currentLine++, 0, "%-8d %-6d %-16ld %-14ld %-14ld",
                    me->monitor[i].pid,
                    -1 - PID_getPriority(me->monitor[i].pid),
                    me->monitor[i].metrics.lastWorkTime / 1000,
                    me->monitor[i].metrics.minWorkTime / 1000,
                    me->monitor[i].metrics.maxWorkTime / 1000
                    );
            }
        }
        pthread_mutex_unlock(&me->isTaskBeingTraced_mutex);
        
        mvprintw(currentLine++, 0, "\n"); // Newline

        // 3º Mostra todos os outros tipos de tarefas
        mvprintw(currentLine++, 0, "Other policys:");

        // Imprimir cabeçalho
        mvprintw(currentLine++, 0, "PID      SCHEDTYPE       PRI    WORKTIME(us)    MINTIME(us)     MAXTIME(us)");

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
                mvprintw(currentLine++, 0, "%-8d %-15s %-6d %-16ld %-14ld %-14ld",
                    me->monitor[i].pid,
                    schedulerPolicy,
                    PID_getPriority(me->monitor[i].pid),
                    me->monitor[i].metrics.lastWorkTime / 1000,
                    me->monitor[i].metrics.minWorkTime / 1000,
                    me->monitor[i].metrics.maxWorkTime / 1000
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