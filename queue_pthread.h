#include <stdbool.h>
#include <stdio.h>
#include <pthread.h>

typedef struct queue
{
    pthread_t *arr;
    int tail;
    size_t element_size;
} Queue;

Queue *init_queue()
{
    Queue *queue = malloc(sizeof(Queue));

    queue->arr = NULL;
    queue->tail = -1;
    queue->element_size = sizeof(pthread_t);

    return queue;
}

static void realloc_arr(Queue *queue)
{
    pthread_t *temp = realloc(queue->arr, queue->element_size * (queue->tail + 1));

    queue->arr = temp;
}

void enqueue(Queue *queue, pthread_t val)
{
    queue->tail++;
    realloc_arr(queue);
    queue->arr[queue->tail] = val;
}

pthread_t dequeue(Queue *queue)
{
    pthread_t val = queue->arr[0];

    for (int i = 1; i <= queue->tail; i++)
    {
        queue->arr[i - 1] = queue->arr[i];
    }

    queue->arr[queue->tail] = 0;

    queue->tail--;

    if (queue->tail >= 0)
    {
        realloc_arr(queue);
    }
    else
    {
        free(queue->arr);
        queue->arr = NULL;
    }

    return val;
}

int queue_size(Queue *queue)
{
    return queue->tail + 1;
}

void free_queue(Queue *queue)
{
    free(queue->arr);
    free(queue);
}