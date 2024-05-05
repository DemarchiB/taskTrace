#include "../TaskTrace/TaskTrace.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h> // getpid

#define NUM_THREADS 50

void *initTime_Benchmark_task(void *arg)
{
    (void) (arg);
    TaskTrace taskTrace;
    pid_t pid = PID_get();
    // printf("initTime_Benchmark_task: Task created with pid %d\n", pid);

    if (TaskTrace_init(&taskTrace)) {
        printf("initTime_Benchmark_task %d: Error initializing taskTrace\n", pid);
        return NULL;
    }

    if (TaskTrace_enableRecording(&taskTrace)) {
        printf("initTime_Benchmark_task %d: Start recording returned error\n", pid);
        return NULL;
    }

    uint64_t runtimeInNs = 10 * 1000 * 1000;   // 10 ms reserved
    uint64_t deadlineInNs = 10 * 1000 * 1000;  // 10 ms deadline
    uint64_t periodInNs = 1000 * 1000 * 1000;   // 1s period
    
    struct timespec t1, t2;
    if (PID_setDeadline(pid, runtimeInNs, deadlineInNs, periodInNs)) {
        printf("initTime_Benchmark_task %d: Error changing scheduler to deadline: \n", pid);
        perror("");
        return NULL;
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
    TaskTrace_traceDeadlineTaskStartPoint(&taskTrace);

    clock_gettime(CLOCK_MONOTONIC_RAW, &t2);
    
    printf("initTime_Benchmark_task %d: benchtime = %lu us\n", pid, (((t2.tv_sec * 1000 * 1000 * 1000) + (t2.tv_nsec)) - ((t1.tv_sec * 1000 * 1000 * 1000) + (t1.tv_nsec))) / 1000);

    while (1) {
        sleep(1);
    }

    return NULL;
}

int main() {
    pthread_t tasks[NUM_THREADS];
    int taskNumber[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        taskNumber[i] = i;
    }

    printf("initTime_Benchmark: Started with PID %d\n", PID_get());

    printf("initTime_Benchmark: Creating threads\n");

    // Criação das tasks
    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_create(&tasks[i], NULL, initTime_Benchmark_task, &taskNumber[i]) != 0) {
            perror("Erro ao criar a tarefa de usuário");
            exit(EXIT_FAILURE);
        }
    }

    printf("initTime_Benchmark: Waiting threads to finish\n");
    // Espera pelas tasks terminarem
    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_join(tasks[i], NULL) != 0) {
            perror("Erro ao aguardar as tarefas do usuário finalizarem");
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}
