#include "utilities.h"
#include <sys/wait.h>
#include <signal.h>
#include <stdbool.h>
#include "queue.h"

#define PASSENGERS_NUMBER 60
#define GATE_NUM 3
#define FERRY_NUM 3
#define FERRY_START 30

void set_env_var(char *name, char *value)
{
    if (setenv(name, value, 1) < 0)
    {
        log_error("ORCHESTRATOR", "Failure while creating ipc");
        exit(-1);
    }
}

Queue *ferry_pids;
Queue *travelling_ferry_pids;
bool sleep_break = false;
bool forced_ferry_leave = false;
int current_ferry;
int passengers_left = PASSENGERS_NUMBER;
char strBuff[100];

void signal_handler(int signum, siginfo_t *info, void *ptr)
{
    if (signum == SIGTERM)
    {
        int id = dequeue(travelling_ferry_pids);
        enqueue(ferry_pids, id);
        sprintf(strBuff, "Ferry %d returned from course", id);
        log_info("ORCHESTRATOR", strBuff);
    }
    else if (signum == SIGQUIT)
    {
        kill(current_ferry, SIGPIPE);
        sleep_break = true;
        forced_ferry_leave = true;
    }
}

void sleep_interruptable(int seconds)
{
    while (seconds > 0)
    {
        if (!sleep_break)
        {
            sleep(1);
        }
        else
        {
            break;
        }
        seconds--;
    }
    sleep_break = false;
}

int main()
{
    char log_file_buff[100];

    time_t now = time(NULL);
    struct tm *local_time = localtime(&now);

    strftime(strBuff, sizeof(strBuff), "LOGS_%Y_%m_%d_%H_%M_%S.txt", local_time);

    if (setenv(LOG_ENV, strBuff, 1) < 0)
    {
        perror("Problem with setting env");
        exit(-1);
    }

    int semKey = ftok(".", 'A');

    int semId;

    if ((semId = semget(semKey, 13, IPC_CREAT | 0600)) < 0)
    {
        perror("Error while creating semaphore");
        exit(-1);
    }

    char keyBuff[100];

    sprintf(strBuff, "%d", semId);

    set_env_var(SEM_ENV, strBuff);

    semctl(semId, SEM_IPC, SETVAL, 1);
    semctl(semId, SEM_LOG, SETVAL, 1);
    semctl(semId, SEM_FERRY, SETVAL, 1);
    semctl(semId, SEM_TAKE_PASSENGERS, SETVAL, 0);
    semctl(semId, SEM_LEAVE_PORT, SETVAL, 0);
    semctl(semId, SEM_END, SETVAL, 0);
    semctl(semId, SEM_IPC2, SETVAL, 1);
    semctl(semId, SEM_MAX_LUGGAGE_SHM, SETVAL, 1);
    semctl(semId, SEM_FERRY_START, SETVAL, 0);
    semctl(semId, SEM_AVAILABLE_FERRIES, SETVAL, FERRY_NUM);

    load_sem_id();

    int ipcKey = ftok(".", 'B');

    int ipcId;

    if ((ipcId = msgget(ipcKey, IPC_CREAT | 0600)) < 0)
    {
        log_error("ORCHESTRATOR", "Failure while creating ipc");
        exit(-1);
    }

    sprintf(strBuff, "%d", ipcId);

    set_env_var(IPC_ENV, strBuff);

    ipcKey = ftok(".", 'C');

    ipcId;

    if ((ipcId = msgget(ipcKey, IPC_CREAT | 0600)) < 0)
    {
        log_error("ORCHESTRATOR", "Failure while creating ipc");
        exit(-1);
    }

    sprintf(strBuff, "%d", ipcId);

    set_env_var(WAITING_ROOM_ID, strBuff);

    int shm_key = ftok(".", 'D');

    int shm_id = shmget(shm_key, sizeof(int), IPC_CREAT | 0600);
    if (shm_id < 0)
    {
        log_error("ORCHESTRATOR", "Failure while creating shm");
        exit(-1);
    }

    sprintf(strBuff, "%d", shm_id);

    set_env_var(SHM_LUGGAGE_ENV, strBuff);

    ferry_pids = init_queue(sizeof(int));
    travelling_ferry_pids = init_queue(sizeof(int));

    for (int i = 0; i < FERRY_NUM; i++)
    {
        int id = fork();
        switch (id)
        {
        case -1:
            perror("Fork error");
            exit(-1);
            break;
        case 0:
            execl("ferry.out", "", NULL);
            exit(0);
            break;
        }
        enqueue(ferry_pids, id);
        sleep(1);
    }

    for (int i = 0; i < PASSENGERS_NUMBER; i++)
    {
        int id = fork();
        switch (id)
        {
        case -1:
            perror("Fork error");
            exit(-1);
            break;
        case 0:
            execl("passenger.out", "", NULL);
            exit(0);
            break;
        }
        sleep(1);
    }

    for (int i = 0; i < GATE_NUM; i++)
    {
        int id = fork();
        if (id < 0)
        {
            perror("Fork error");
            exit(-1);
        }
        else if (id == 0)
        {
            execl("boarding_gate.out", "", NULL);
            exit(0);
        }
    }

    sigset_t sigs;

    sigemptyset(&sigs);
    sigaddset(&sigs, SIGQUIT);

    struct sigaction sa;
    sa.sa_sigaction = signal_handler;
    sa.sa_mask = sigs;

    sigaction(SIGTERM, &sa, NULL);

    while (passengers_left > 0)
    {
        if(queue_size(ferry_pids) < 1)
        {
            pause();
            continue;
        }
        current_ferry = dequeue(ferry_pids);
        forced_ferry_leave = false;

        kill(current_ferry, SIGSYS);
        sleep_interruptable(20);
        kill(current_ferry, SIGTERM);
        sleep_interruptable(30);

        sem_v(semId, SEM_LEAVE_PORT);
        if (!forced_ferry_leave)
        {
            kill(current_ferry, SIGPIPE);
        }

        enqueue(travelling_ferry_pids, current_ferry);
        sleep(1);
    }

    free_queue(travelling_ferry_pids);
    free_queue(ferry_pids);

    log_info("ORCHESTRATOR", "No passengers left");

    sem_v(semId, SEM_END);

    log_info("ORCHESTRATOR", "IPC closed");

    if (semctl(semId, 0, IPC_RMID) < 0)
    {
        perror("Failed to delete semaphore");
        exit(-1);
    }

    log_info("ORCHESTRATOR", "Semaphore closed");

    return 0;
}