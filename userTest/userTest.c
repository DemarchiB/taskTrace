#include "../TaskTrace/TaskTrace.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h> // getpid
#include <errno.h>
#include <sys/timerfd.h>

#define USE_BENCHMARK_TASKS 1
#if (USE_BENCHMARK_TASKS == 1)
#define NUM_BENCHMARK_TASKS 5
#else
#define NUM_BENCHMARK_TASKS 0
#endif
#define NUM_DEFAULT_DEADLINE_TASKS 1
#define NUM_FIFO_THREADS 0

#if (USE_BENCHMARK_TASKS == 1)
#define NUM_THREADS NUM_DEFAULT_DEADLINE_TASKS + NUM_FIFO_THREADS + NUM_BENCHMARK_TASKS
#else
#define NUM_THREADS NUM_DEFAULT_DEADLINE_TASKS + NUM_FIFO_THREADS
#endif

typedef struct {
    int taskNumber;
    uint64_t runtimeInNs;   // reserved runtime
    uint64_t deadlineInNs;  // deadline
    uint64_t periodInNs;   // period
    long int workTimeInNs;
    bool useTimerfd;
    bool useNanosleep;
} TaskArg;

const uint64_t defaultRuntimeInNs = 1.6 * 1000 * 1000;   // reserved runtime
const uint64_t defaultDeadlineInNs = 4 * 1000 * 1000;  // deadline
const uint64_t defaultPeriodInNs = 5 * 1000 * 1000;   // period
const long int defaultWorkTimeInNs = (long int) 1.5 * 1000 * 1000;

static inline void add_timespec(struct timespec *t, long ns)
{
    t->tv_nsec += ns;
    while (t->tv_nsec >= 1000000000L) {
        t->tv_nsec -= 1000000000L;
        t->tv_sec += 1;
    }
}

void *user_deadline_task(void *arg)
{
    const TaskArg *const taskArg = arg;
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
    
    if (PID_setDeadline(pid, taskArg->runtimeInNs, taskArg->deadlineInNs, taskArg->periodInNs)) {
        printf("UserTask %d: Error changing scheduler to deadline\n", pid);
        perror("");
        return NULL;
    }

    struct timespec next_release = {0}; // only used if taskArg->useNanosleep
    int fd = 0; // only used if taskArg->useNanosleep

    if (taskArg->useNanosleep) {
        next_release.tv_sec = 0;
        next_release.tv_nsec = 0;
    } else if (taskArg->useTimerfd) {
        fd = timerfd_create(CLOCK_MONOTONIC, 0);

        struct itimerspec its = {
            .it_interval = {0, taskArg->periodInNs}, // 500 ms
            .it_value    = {0, taskArg->periodInNs}
        };

        timerfd_settime(fd, 0, &its, NULL);
    }

    while(1) {
        struct timespec t_ini, t_cur;
        TaskTrace_traceExecutionStart(&taskTrace);
        
        // Read the actual time
        clock_gettime(CLOCK_MONOTONIC, &t_ini);
        t_cur.tv_nsec = t_ini.tv_nsec;
        t_cur.tv_sec = t_cur.tv_sec;

        // simulate some load
        while ((((t_cur.tv_sec * 1000 * 1000 * 1000) + t_cur.tv_nsec) - ((t_ini.tv_sec * 1000 * 1000 * 1000) + t_ini.tv_nsec)) < taskArg->workTimeInNs) {
            clock_gettime(CLOCK_MONOTONIC, &t_cur);
        }

        TaskTrace_traceExecutionStop(&taskTrace);

        if (taskArg->useNanosleep) {
            if ((next_release.tv_sec == 0) && (next_release.tv_nsec == 0)) {
                next_release.tv_sec = t_ini.tv_sec;
                next_release.tv_nsec = t_ini.tv_nsec;
            }
            // Calcula próxima liberação do período
            add_timespec(&next_release, taskArg->periodInNs);

            // Espera até o instante ABSOLUTO
            if (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_release, NULL)) {
                int saved = errno;
                printf("clock_nanosleep %d error %d\n", pid, saved);
            }
        } else if (taskArg->useNanosleep) {
            uint64_t expirations;
            read(fd, &expirations, sizeof(expirations));
        } else {
            sched_yield();
        }
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

    TaskArg defaultTask[NUM_DEFAULT_DEADLINE_TASKS] = {0};

    for (int i = 0; i < NUM_DEFAULT_DEADLINE_TASKS; i++) {
        defaultTask[i].taskNumber = taskCount++;
        defaultTask[i].runtimeInNs = defaultRuntimeInNs;
        defaultTask[i].deadlineInNs = defaultDeadlineInNs;
        defaultTask[i].periodInNs = defaultPeriodInNs;
        defaultTask[i].workTimeInNs = defaultWorkTimeInNs;
        if (i == 1) {
            defaultTask[i].useNanosleep = true;
        } else if (i == 2) {
            defaultTask[i].useTimerfd = true;
        }
    }

#if (USE_BENCHMARK_TASKS == 1)
    TaskArg benchmarkTask[NUM_BENCHMARK_TASKS] = {0};
    benchmarkTask[0].taskNumber = taskCount++;
    benchmarkTask[0].runtimeInNs = 2 * 1000 * 1000;
    benchmarkTask[0].deadlineInNs = 5 * 1000 * 1000;
    benchmarkTask[0].periodInNs = 5 * 1000 * 1000;
    benchmarkTask[0].workTimeInNs = 1 * 1000 * 1000;

    benchmarkTask[1].taskNumber = taskCount++;
    benchmarkTask[1].runtimeInNs = 2 * 1000 * 1000;
    benchmarkTask[1].deadlineInNs = 5 * 1000 * 1000;
    benchmarkTask[1].periodInNs = 5 * 1000 * 1000;
    benchmarkTask[1].workTimeInNs = 2 * 1000 * 1000;

    benchmarkTask[2].taskNumber = taskCount++;
    benchmarkTask[2].runtimeInNs = 2 * 1000 * 1000;
    benchmarkTask[2].deadlineInNs = 2 * 1000 * 1000;
    benchmarkTask[2].periodInNs = 5 * 1000 * 1000;
    benchmarkTask[2].workTimeInNs = 1 * 1000 * 1000;

    benchmarkTask[3].taskNumber = taskCount++;
    benchmarkTask[3].runtimeInNs = 2 * 1000 * 1000;
    benchmarkTask[3].deadlineInNs = 2 * 1000 * 1000;
    benchmarkTask[3].periodInNs = 2 * 1000 * 1000;
    benchmarkTask[3].workTimeInNs = 1 * 1000 * 1000;

    benchmarkTask[4].taskNumber = taskCount++;
    benchmarkTask[4].runtimeInNs = 2 * 1000 * 1000;
    benchmarkTask[4].deadlineInNs = 2 * 1000 * 1000;
    benchmarkTask[4].periodInNs = 2 * 1000 * 1000;
    benchmarkTask[4].workTimeInNs = 1.5 * 1000 * 1000;

#endif

    for (int i = 0; i < NUM_THREADS; i++) {
        taskNumber[i] = i;
    }
    taskCount = 0;

    printf("UserTest: Started with PID %d\n", PID_get());

    printf("UserTest: Creating threads\n");

    // Criação das tasks deadline
    for (int i = 0; i < NUM_DEFAULT_DEADLINE_TASKS; i++, taskCount++) {
        if (pthread_create(&tasks[taskCount], NULL, user_deadline_task, &defaultTask[i]) != 0) {
            perror("Erro ao criar a tarefa de usuário");
            exit(EXIT_FAILURE);
        }
    }

#if (USE_BENCHMARK_TASKS == 1)
    for (int i = 0; i < NUM_BENCHMARK_TASKS; i++, taskCount++) {
        if (pthread_create(&tasks[taskCount], NULL, user_deadline_task, &benchmarkTask[i]) != 0) {
            perror("Erro ao criar a tarefa de usuário");
            exit(EXIT_FAILURE);
        }
    }
#endif
    
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
