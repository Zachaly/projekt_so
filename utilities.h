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
#include "errno.h"

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
            perror("Semaphore error!(p)");
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
        if (errno == EINTR)
        {
            sem_v(sem_num);
            return;
        }
        perror("Semaphore error!(v)");
        exit(-1);
    }
}

void log_info(char *source, char *text)
{
    time_t now = time(NULL);
    struct tm local_time;
    localtime_r(&now, &local_time);

    sem_p(SEM_LOG);
    int file = open(file_name, O_WRONLY | O_CREAT | O_APPEND, 0644);

    if (!file)
    {
        perror("Cannot open log file");
    }

    char time_buffer[50];

    char buff[300];
    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", &local_time);

    int len = snprintf(buff, sizeof(buff), "[%s][%s](%d): %s\n", time_buffer, source, getpid(), text);

    if (write(file, buff, len) < 0)
    {
        perror("Error while writing to file");
    }

    if (close(file) != 0)
    {
        perror("Failed to close log file");
    }

    printf("[%s][%s](%d): %s\n", time_buffer, source, getpid(), text);
    sem_v(SEM_LOG);
}

int random_number(int min, int max)
{
    return rand() % (max + 1 - min) + min;
}

void custom_sleep(int t)
{
    return;
    time_t start;
    time(&start);

    time_t current;
    time(&current);

    while (current - start < t)
    {
        time(&current);
    }
}