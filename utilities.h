#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/shm.h>
#include "config.h"

int sem_id;
char *file_name;

void load_sem_id()
{
    sem_id = atoi(getenv(SEM_ENV));
    file_name = getenv(LOG_ENV);
}

void sem_p(int sem_num)
{
    struct sembuf buf;
    buf.sem_num = sem_num;
    buf.sem_op = -1;
    buf.sem_flg = 0;

    if (semop(sem_id, &buf, 1) == -1)
    {
        if (errno == EINTR)
        {
            sem_p(sem_num);
        }
        else
        {
            perror("Semaphore error!");
            exit(-1);
        }
    }
}

void sem_v(int sem_num)
{
    struct sembuf buf;
    buf.sem_num = sem_num;
    buf.sem_op = 1;
    buf.sem_flg = 0;

    if (semop(sem_id, &buf, 1) == -1)
    {
        perror("Semaphore error!");
        exit(-1);
    }
}

void log_info(char *source, char *text)
{
    time_t now = time(NULL);
    struct tm *local_time = localtime(&now);

    sem_p(SEM_LOG);

    FILE *file = fopen(file_name, "a");

    if (!file)
    {
        perror("Cannot open log file");
        exit(-1);
    }

    char time_buffer[50];

    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", local_time);

    fprintf(file, "[%s][%s](%d): %s\n", time_buffer, source, getpid(), text);
    
    fclose(file);
    printf("[%s][%s](%d): %s\n", time_buffer, source, getpid(), text);

    sem_v(SEM_LOG);
}

int random_number(int min, int max)
{
    return rand() % (max + 1 - min) + min;
}

void custom_sleep(int t)
{
    //return;
    time_t start;
    time(&start);

    time_t current;
    time(&current);

    while (current - start < t)
    {
        time(&current);
    }
}