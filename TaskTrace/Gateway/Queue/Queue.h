#ifndef __TASK_TRACE_GATEWAY_QUEUE_H__
#define __TASK_TRACE_GATEWAY_QUEUE_H__

#include "../../../Common/Telegram/Telegram.h"

#include <semaphore.h>

#define QUEUE_SIZE 1000  // Defina o tamanho do buffer cíclico

typedef struct {
    Telegram telegram[QUEUE_SIZE];  // Armazena os dados
    int head;                     // Índice do próximo elemento a ser lido
    int tail;                     // Índice onde o próximo dado será adicionado
    int count;                    // Número de elementos na fila
    sem_t sem;                    // Semáforo de contagem
} Queue;

void Queue_init(Queue *const me);
int Queue_add(Queue *const me, const Telegram *const telegram);
int Queue_getNext(Queue *const me, Telegram* const telegram);

#endif // __TASK_TRACE_GATEWAY_QUEUE_H__