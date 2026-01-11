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

#define LOG_ENV "CURRENT_RUN_LOG"
#define IPC_ENV "IPC_ID"
#define SEM_ENV "SEM_ID"
#define FIFO_NAME "./fifo"
#define SEM_IPC 1
#define SEM_LOG 2
#define SEM_FERRY 3
#define SEM_FERRY_CAP 4
#define SEM_END 5
#define SEM_LEAVE_PORT 6
#define SEM_TAKE_PASSENGERS 7
#define WAITING_ROOM_ID "IPC2_ID"
#define SEM_IPC2 8
#define FERRY_CAPACITY 10
#define SEM_GANGWAY 9
#define GANGWAY_CAP 5
#define SEM_MAX_LUGGAGE_SHM 10
#define SHM_LUGGAGE_ENV "SHM_LUGGAGE"
#define SEM_FERRY_START 11
#define SEM_SHM_PASSENGERS 12
#define SHM_PASSENGERS_ENV "SHM_PASSENGERS"

int semId;

static char time_buffer[100];

void load_sem_id()
{
    semId = atoi(getenv(SEM_ENV));
}

void sem_p(int sem_id, int sem_num) {
    struct sembuf buf;
    buf.sem_num = sem_num;
    buf.sem_op = -1;
    buf.sem_flg = 0;

    if (semop(sem_id, &buf, 1) == -1) {
        if (errno == EINTR) {
            sem_p(sem_id, sem_num);
        }
        else {
            perror("Blad opuszczania semafora!");
            exit(-1);
        }
    }
}

void sem_v(int sem_id, int sem_num) {
    struct sembuf buf;
    buf.sem_num = sem_num;
    buf.sem_op = 1;
    buf.sem_flg = 0;

    if (semop(sem_id, &buf, 1) == -1) {
        perror("Blad podnoszenia semafora!");
        exit(-1);
    }
}

void log_info(char *source, char *text)
{
    char *file_name = getenv(LOG_ENV);

    sem_p(semId, SEM_LOG);

    FILE *file = fopen(file_name, "a");

    if (!file)
    {
        perror("Cannot open log file");
        exit(-1);
    }

    time_t now = time(NULL);
    struct tm *local_time = localtime(&now);

    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", local_time);

    fprintf(file, "[%s][%s](%d): %s\n", time_buffer, source, getpid(), text);
    fclose(file);

    printf("[%s][%s](%d): %s\n", time_buffer, source, getpid(), text);

    sem_v(semId, SEM_LOG);
}

void log_error(char *source, char *text)
{
    char *file_name = getenv(LOG_ENV);

    sem_p(semId, SEM_LOG);

    FILE *file = fopen(file_name, "a");

    if (!file)
    {
        perror("Cannot open log file");
        exit(-1);
    }

    time_t now = time(NULL);
    struct tm *local_time = localtime(&now);

    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", local_time);

    fprintf(file, "[%s][%s](%d): %s\n", time_buffer, source, getpid(), text);
    fclose(file);
    perror(text);

    sem_v(semId, SEM_LOG);
}

int random_number(int min, int max)
{
    return (rand() % max) + min;
}