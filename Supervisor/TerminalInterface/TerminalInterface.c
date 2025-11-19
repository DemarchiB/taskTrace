#include "TerminalInterface.h"

#include <stdio.h>
#include <string.h>
#include <dirent.h>     // opendir, readdir, closedir
#include <stdlib.h>     // atoi
#include <sys/mman.h>   // mlockall
#include <unistd.h>     // usleep, getuid

#include <ncurses.h>    //  Interface

static void *TerminalInterface_interfaceUpdateTask(void *arg);

int TerminalInterface_start(TerminalInterface *const me, const volatile Supervisor *const supervisor)
{
    me->supervisor = supervisor;

    if (pthread_create(&me->interfaceUpdateTask_id, NULL, TerminalInterface_interfaceUpdateTask, me) != 0) {
        return -1;
    }

    return 0;
}

static void *TerminalInterface_interfaceUpdateTask(void *arg)
{
    TerminalInterface *const me = arg;
    Supervisor *const supervisor = me->supervisor;

    printf("Supervisor: Interface updater task created\n");

    sleep(3);

    // Inicializar o modo ncurses
    initscr();
    cbreak(); // Desabilitar buffering de linha
    noecho(); // Não exibir caracteres digitados
    curs_set(0); // Ocultar cursor

    char schedulerPolicy[20];
    int currentLine;
    uint64_t runtime, period, deadline;

    while (1) {
        clear(); // Limpar a tela

        currentLine = 0;
        mvprintw(currentLine++, 0, "%-20s %-1s %-1d",
            "User inputs->",
            "logging:",
            supervisor->userInputs.enableLogging
            );

        // 1º Mostra tarefas do tipo deadline
        mvprintw(currentLine++, 0, "SCHED_DEADLINE TASKS:");
        mvprintw(currentLine++, 0, "PID      PRI    RESPONSE_TIME(ms)    WCRT(ms)    Latency(us)   maxLat(us)  adjusted(us)  cycles     dlLosts  rtOverrun  depleted   RUNTIME(ms)    DEADLINE(ms)    PERIOD(ms)    RUN%%    PROC%%");

        pthread_mutex_lock(&supervisor->isTaskBeingTraced_mutex);
        for (ssize_t i = 0; i < MAX_TRACED_TASKS; i++) {
            if (supervisor->isTaskBeingTraced[i]) {
                int policy = PID_getSchedulerPolicy(supervisor->monitor[i].pid, NULL);
                if (policy != SCHED_DEADLINE) { // 6 = sched_deadline
                    continue;
                }

                if (PID_getDeadlinePropeties(supervisor->monitor[i].pid, &runtime, &deadline, &period) != 0) {
                    runtime = 0;
                    deadline = 0;
                    period = 0;
                }

                // Printa dados das tarefas
                mvprintw(currentLine++, 0, "%-8d %-6s %-20.3f %-11.3f %-13.1f %-11.1f %-13.1f %-10d %-8d %-10d %-10d %-14.3f %-15.3f %-13.3f %-8.1f %-3.1f",
                    supervisor->monitor[i].pid,
                    "rt",
                    (float) (supervisor->monitor[i].metrics.lastRT / (1000 * 1000.0)),
                    (float) (supervisor->monitor[i].metrics.WCRT / (1000 * 1000.0)),
                    (float) (supervisor->monitor[i].metrics.lastLatency / (1000.0)),
                    (float) (supervisor->monitor[i].metrics.maxLatency / (1000.0)),
                    (float) ((float)supervisor->monitor[i].metrics.totalAltoAdjust / (1000.0)),
                    (uint32_t) (supervisor->monitor[i].metrics.cycleCount),
                    (uint32_t) (supervisor->monitor[i].metrics.deadlineLostCount),
                    (uint32_t) (supervisor->monitor[i].metrics.runtimeOverrunCount),
                    (uint32_t) (supervisor->monitor[i].metrics.taskDepletedCount),
                    (float) (runtime / (1000 * 1000.0)),
                    (float) (deadline / (1000 * 1000.0)),
                    (float) (period / (1000 * 1000.0)),
                    (float) (supervisor->monitor[i].metrics.WCRT * 100.0 / runtime),
                    (float) (supervisor->monitor[i].metrics.WCRT * 100.0 / runtime) * ( (float) runtime / (float) period)
                    );
            }
        }
        pthread_mutex_unlock(&supervisor->isTaskBeingTraced_mutex);

        mvprintw(currentLine++, 0, "\n"); // Newline

        // 2º Mostra tarefas do tipo FIFO
        mvprintw(currentLine++, 0, "SCHED_FIFO TASKS:");

        // Imprimir cabeçalho
        mvprintw(currentLine++, 0, "PID      PRI    RESPONSE_TIME(ms)    MIN_RT(ms)  WCRT(ms)");

        pthread_mutex_lock(&supervisor->isTaskBeingTraced_mutex);
        for (ssize_t i = 0; i < MAX_TRACED_TASKS; i++) {
            if (supervisor->isTaskBeingTraced[i]) {
                int policy = PID_getSchedulerPolicy(supervisor->monitor[i].pid, NULL);
                if (policy != SCHED_FIFO) {
                    continue;
                }

                // Printa dados das tarefas
                mvprintw(currentLine++, 0, "%-8d %-6d %-20.3f %-11.3f %-13.3f",
                    supervisor->monitor[i].pid,
                    -1 - PID_getPriority(supervisor->monitor[i].pid),
                    (float) (supervisor->monitor[i].metrics.lastRT / (1000 * 1000.0)),
                    (float) (supervisor->monitor[i].metrics.minRT / (1000 * 1000.0)),
                    (float) (supervisor->monitor[i].metrics.WCRT / (1000 * 1000.0))
                    );
            }
        }
        pthread_mutex_unlock(&supervisor->isTaskBeingTraced_mutex);
        
        mvprintw(currentLine++, 0, "\n"); // Newline

        // 3º Mostra todos os outros tipos de tarefas
        mvprintw(currentLine++, 0, "Other policys:");

        // Imprimir cabeçalho
        mvprintw(currentLine++, 0, "PID      SCHEDTYPE       PRI    RESPONSE_TIME(ms)    MIN_RT(ms)  WCRT(ms)");

        pthread_mutex_lock(&supervisor->isTaskBeingTraced_mutex);
        for (ssize_t i = 0; i < MAX_TRACED_TASKS; i++) {
            if (supervisor->isTaskBeingTraced[i]) {
                int policy = PID_getSchedulerPolicy(supervisor->monitor[i].pid, schedulerPolicy);
                if ((policy == SCHED_FIFO) || (policy == 6)) {
                    continue;
                }

                if (policy < 0) {
                    snprintf(schedulerPolicy, sizeof(schedulerPolicy), "UNKNOWN");
                }

                // Printa dados das tarefas
                mvprintw(currentLine++, 0, "%-8d %-15s %-6d %-20.3f %-11.3f %-13.3f",
                    supervisor->monitor[i].pid,
                    schedulerPolicy,
                    PID_getPriority(supervisor->monitor[i].pid),
                    (float) (supervisor->monitor[i].metrics.lastRT / (1000 * 1000.0)),
                    (float) (supervisor->monitor[i].metrics.minRT / (1000 * 1000.0)),
                    (float) (supervisor->monitor[i].metrics.WCRT / (1000 * 1000.0))
                    );
            }
        }
        pthread_mutex_unlock(&supervisor->isTaskBeingTraced_mutex);

        refresh(); // Atualizar a tela

        // Aguardar um segundo
        sleep(1);
    }

    pthread_exit(NULL);
}