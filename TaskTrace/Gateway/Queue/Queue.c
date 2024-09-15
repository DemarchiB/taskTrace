#include "Queue.h"
#include <string.h>

// Função para inicializar a fila e o semáforo
void Queue_init(Queue *const me)
{
    me->head = 0;
    me->tail = 0;
    me->count = 0;
    sem_init(&me->sem, 0, 0);  // Inicializa o semáforo com valor 0
}

// Função para adicionar um dado na fila e incrementar o semáforo
int Queue_add(Queue *const me, const Telegram *const telegram)
{
    if (me->count >= QUEUE_SIZE) {
        return -1;  // Fila cheia
    }
    memcpy(&me->telegram[me->tail], telegram, sizeof(Telegram));
    me->tail = (me->tail + 1) % QUEUE_SIZE;  // Move o índice circularmente
    me->count++;
    sem_post(&me->sem);  // Incrementa o semáforo
    return 0;  // Sucesso
}

// Função para pegar o próximo dado da fila e decrementar o semáforo
int Queue_getNext(Queue *const me, Telegram* const telegram)
{
    sem_wait(&me->sem);  // Decrementa o semáforo, espera até que seja maior que 0
    if (me->count <= 0) {
        return -1;  // Fila vazia
    }
    memcpy(telegram, &me->telegram[me->head], sizeof(Telegram)); // Copia os dados do buffer para o ponteiro
    me->head = (me->head + 1) % QUEUE_SIZE;  // Move o índice circularmente
    me->count--;
    return 0;  // Sucesso
}