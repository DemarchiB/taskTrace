#include "../TaskTrace/TaskTrace.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h> // getpid

#define NUM_DEADLINE_THREADS 3
#define NUM_FIFO_THREADS 0
#define NUM_THREADS NUM_DEADLINE_THREADS + NUM_FIFO_THREADS

void *user_deadline_task(void *arg)
{
    int taskNumber = *(int *)arg;
    (void) taskNumber;
    TaskTrace taskTrace;
    pid_t pid = PID_get();
    printf("UserTask: Deadline task created with pid %d\n", pid);

    if (TaskTrace_init(&taskTrace)) {
        printf("UserTask %d: Error initializing taskTrace\n", pid);
        return NULL;
    }

    if (TaskTrace_enableRecording(&taskTrace)) {
        printf("UserTask %d: Start recording returned error\n", pid);
        return NULL;
    }

    uint64_t runtimeInNs = 5 * 1000 * 1000;   // reserved runtime
    uint64_t deadlineInNs = 5 * 1000 * 1000;  // deadline
    uint64_t periodInNs = 15 * 1000 * 1000;   // period

    // const uint64_t workTime = (uint64_t) pid * 100;
    const long int workTimeInNs = (long int) 3 * 1000 * 1000;
    
    if (PID_setDeadline(pid, runtimeInNs, deadlineInNs, periodInNs)) {
        printf("UserTask %d: Error changing scheduler to deadline: ", pid);
        perror("");
        return NULL;
    }

    while(1) {
        struct timespec t_ini, t_cur;
        TaskTrace_traceExecutionStart(&taskTrace);
        
        // Read the actual time
        clock_gettime(CLOCK_MONOTONIC_RAW, &t_ini);
        t_cur.tv_nsec = t_ini.tv_nsec;
        t_cur.tv_sec = t_cur.tv_sec;

        // simulate some load
        while ((((t_cur.tv_sec * 1000 * 1000 * 1000) + t_cur.tv_nsec) - ((t_ini.tv_sec * 1000 * 1000 * 1000) + t_ini.tv_nsec)) < workTimeInNs) {
            clock_gettime(CLOCK_MONOTONIC_RAW, &t_cur);
        }

        TaskTrace_traceExecutionStop(&taskTrace);

        sched_yield();
    }

    return NULL;
}

void *user_fifo_task(void *arg)
{
    int taskNumber = *(int *)arg;
    (void) taskNumber;
    TaskTrace taskTrace;
    pid_t pid = PID_get();
    printf("UserTask: FIFO task created with pid %d\n", pid);

    if (TaskTrace_init(&taskTrace)) {
        printf("UserTask %d: Error initializing taskTrace\n", pid);
        return NULL;
    }

    if (TaskTrace_enableRecording(&taskTrace)) {
        printf("UserTask %d: Start recording returned error\n", pid);
        return NULL;
    }

    // Changing scheduler to SCHED_FIFO (RT)
    struct sched_param param = {60};
    if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
        printf("UserTask %d: Error changing scheduler to SCHED_FIFO: ", pid);
        perror("");
        return NULL;
    }

    // const uint64_t workTime = (uint64_t) pid * 100;
    const long int workTimeInNs = (long int) 2 * 1000 * 1000;

    while(1) {
        struct timespec t_ini, t_cur;
        TaskTrace_traceExecutionStart(&taskTrace);
        
        // Read the actual time
        clock_gettime(CLOCK_MONOTONIC_RAW, &t_ini);
        t_cur.tv_nsec = t_ini.tv_nsec;
        t_cur.tv_sec = t_cur.tv_sec;

        // simulate some load
        while ((((t_cur.tv_sec * 1000 * 1000 * 1000) + t_cur.tv_nsec) - ((t_ini.tv_sec * 1000 * 1000 * 1000) + t_ini.tv_nsec)) < workTimeInNs) {
            clock_gettime(CLOCK_MONOTONIC_RAW, &t_cur);
        }

        TaskTrace_traceExecutionStop(&taskTrace);

        sleep(1);
    }

    return NULL;
}

int main() {
    pthread_t tasks[NUM_THREADS];
    int taskNumber[NUM_THREADS];
    int taskCount = 0;

    for (int i = 0; i < NUM_THREADS; i++) {
        taskNumber[i] = i;
    }

    printf("UserTest: Started with PID %d\n", PID_get());

    printf("UserTest: Creating threads\n");

    // Criação das tasks deadline
    for (int i = 0; i < NUM_DEADLINE_THREADS; i++, taskCount++) {
        if (pthread_create(&tasks[taskCount], NULL, user_deadline_task, &taskNumber[taskCount]) != 0) {
            perror("Erro ao criar a tarefa de usuário");
            exit(EXIT_FAILURE);
        }
    }
    
    // Criação das tasks fifo
    for (int i = 0; i < NUM_FIFO_THREADS; i++, taskCount++) {
        if (pthread_create(&tasks[taskCount], NULL, user_fifo_task, &taskNumber[taskCount]) != 0) {
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
