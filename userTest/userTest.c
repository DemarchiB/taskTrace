#include "../TaskTrace/TaskTrace.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h> // getpid

#define NUM_THREADS 3

void *user_task(void *arg) 
{
    int taskNumber = *(int *)arg;
    TaskTrace taskTrace;
    pid_t pid = PID_get();
    printf("UserTask: Task created with pid %d\n", pid);

    if (TaskTrace_init(&taskTrace)) {
        printf("UserTask %d: Error initializing taskTrace\n", pid);
        return NULL;
    }

    if (TaskTrace_enableRecording(&taskTrace)) {
        printf("UserTask %d: Start recording returned error\n", pid);
        return NULL;
    }

    // Odd process will be FIFO, even will be DEADLINE
    if (1) {//(taskNumber%2 == 0) {
        uint64_t runtimeInNs = 10 * 1000 * 1000;   // 100 ms reserved
        uint64_t deadlineInNs = 20 * 1000 * 1000;  // 300 ms deadline
        uint64_t periodInNs = 1000 * 1000 * 1000;   // 1s period
        
        if (PID_setDeadline(pid, runtimeInNs, deadlineInNs, periodInNs)) {
            printf("UserTask %d: Error changing scheduler to deadline: ", pid);
            perror("");
            return NULL;
        }
        TaskTrace_deadlineTaskStartPoint(&taskTrace);
    } else {
        // Changing scheduler to SCHED_FIFO (RT)
        struct sched_param param = {60};
        if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
            printf("UserTask %d: Error changing scheduler to SCHED_FIFO: ", pid);
            perror("");
            return NULL;
        }
    }

    // const uint64_t workTime = (uint64_t) pid * 100;
    const long int workTimeInNs = (long int) 10 * 1000 * 1000;  // 10ms for all tasks

    while(1) {
        struct timespec t_ini, t_cur;
        TaskTrace_traceWorkStart(&taskTrace);
        
        // Read the actual time
        clock_gettime(CLOCK_MONOTONIC_RAW, &t_ini);
        t_cur.tv_nsec = t_ini.tv_nsec;
        t_cur.tv_sec = t_cur.tv_sec;

        // simulate some load
        while ((((t_cur.tv_sec * 1000 * 1000 * 1000) + t_cur.tv_nsec) - ((t_ini.tv_sec * 1000 * 1000 * 1000) + t_ini.tv_nsec)) < workTimeInNs) {
            clock_gettime(CLOCK_MONOTONIC_RAW, &t_cur);
        }

        TaskTrace_traceWorkStop(&taskTrace);

        if (taskNumber%2 == 0) {
            sched_yield();
        } else {
            sleep(1);
        }
    }

    return NULL;
}

int main() {
    pthread_t tasks[NUM_THREADS];
    int taskNumber[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        taskNumber[i] = i;
    }

    printf("UserTest: Started with PID %d\n", PID_get());

    printf("UserTest: Creating threads\n");

    // Criação das tasks
    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_create(&tasks[i], NULL, user_task, &taskNumber[i]) != 0) {
            perror("Erro ao criar a tarefa de usuário");
            exit(EXIT_FAILURE);
        }
    }

    printf("UserTest: Waiting threads to finish\n");
    // Espera pelas tasks terminarem
    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_join(tasks[i], NULL) != 0) {
            perror("Erro ao aguardar as tarefas do usuário finalizarem");
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}
