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

#define LOG_ENV "CURRENT_RUN_LOG"

void sem_p(int sem_id, int sem_num, int op, int flag)
{
    struct sembuf buf;
    buf.sem_num = sem_num;
    buf.sem_op = op;
    buf.sem_flg = flag;

    if (semop(sem_id, &buf, 1) == -1)
    {
        if (errno == EINTR)
        {
            sem_p(sem_id, sem_num, op, flag);
        }
        else
        {
            perror("Blad opuszczania semafora!");
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        printf("Semafor zostal zamkniety. \n");
    }
}

void sem_v(int sem_id, int sem_num, int op, int flag)
{
    struct sembuf buf;
    buf.sem_num = sem_num;
    buf.sem_op = op;
    buf.sem_flg = flag;

    if (semop(sem_id, &buf, 1) == -1)
    {
        perror("Blad podnoszenia semafora!");
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("Semafor zostal otwarty\n");
    }
}

void log_info(char *source, char *text)
{
    char *file_name = getenv(LOG_ENV);

    FILE *file = fopen(file_name, "a");

    if (!file)
    {
        perror("Nie można otworzyć pliku");
        exit(-1);
    }

    time_t now = time(NULL);
    struct tm *local_time = localtime(&now);

    char buffer[100];

    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", local_time);

    fprintf(file, "[%s][%s](%d): %s\n", buffer, source, getpid(), text);

    printf("[%s][%s](%d): %s\n", buffer, source, getpid(), text);
}