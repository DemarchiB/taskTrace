#include "../TaskTrace/TaskTrace.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h> // getpid

#define NUM_THREADS 3

void *user_task(void *arg) 
{
    (void) arg;
    TaskTrace taskTrace;
    struct sched_param param = {60};
    pid_t pid = PID_get();
    printf("UserTask: Task created with pid %d\n", pid);

    // Changing scheduler to SCHED_FIFO (RT)
    if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
        printf("UserTask %d: Error changing scheduler to SCHED_FIFO: ", pid);
        perror("");
        return NULL;
    }

    if (TaskTrace_init(&taskTrace)) {
        printf("UserTask %d: Error initializing taskTrace\n", pid);
        return NULL;
    }

    if (TaskTrace_startRecording(&taskTrace)) {
        printf("UserTask %d: Start recording returned error\n", pid);
        return NULL;
    }

    while(1) {
        TaskTrace_traceWorkStart(&taskTrace);
        usleep(pid);
        TaskTrace_traceWorkStop(&taskTrace);
        sleep(2);
    }

    return NULL;
}

int main() {
    pthread_t tasks[NUM_THREADS];

    // Criação das tasks
    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_create(&tasks[i], NULL, user_task, NULL) != 0) {
            perror("Erro ao criar a tarefa de usuário");
            exit(EXIT_FAILURE);
        }
    }

    // Espera pelas tasks terminarem
    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_join(tasks[i], NULL) != 0) {
            perror("Erro ao aguardar as tarefas do usuário finalizarem");
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}
