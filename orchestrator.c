#include "utilities.h"
#include <sys/wait.h>
#include <signal.h>
#include <stdbool.h>
#include "queue.h"

void set_env_var(char *name, char *value)
{
    if (setenv(name, value, 1) < 0)
    {
        log_error("ORCHESTRATOR", "Failure while creating ipc");
        exit(-1);
    }
}

Queue *available_ferries;
bool custom_sleep_break = false;
bool forced_ferry_leave = false;
int current_ferry;
int *passengers_left;
char strBuff[100];

void signal_handler(int signum, siginfo_t *info, void *context)
{
    if (signum == SIGTERM)
    {
        pid_t id = info->si_pid;
        enqueue(available_ferries, id);
        sprintf(strBuff, "Ferry %d returned from course", id);
        log_info("ORCHESTRATOR", strBuff);
    }
    else if (signum == SIGINT)
    {
        log_info("ORCHESTRATOR", "Captain said that ferry should leave");
        if (semctl(sem_id, SEM_FERRY_CAP, SETVAL, 0) < 0)
        {
            log_error("FERRY", "Failed to set semaphore");
            exit(-1);
        }
        custom_sleep_break = true;
        forced_ferry_leave = true;
    }
}

void custom_sleep_interruptable(int seconds)
{
    while (seconds > 0)
    {
        if (!custom_sleep_break)
        {
            custom_sleep(1);
        }
        else
        {
            break;
        }
        seconds--;
    }
    custom_sleep_break = false;
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
    if (semKey < 0)
    {
        perror("Key error");
        exit(-1);
    }

    sem_id = semget(semKey, 13, IPC_CREAT | 0600);
    if (sem_id < 0)
    {
        perror("Error while creating semaphore");
        exit(-1);
    }

    char keyBuff[100];

    sprintf(strBuff, "%d", sem_id);

    set_env_var(SEM_ENV, strBuff);

    if (semctl(sem_id, SEM_IPC_PASSENGER_QUEUE, SETVAL, 1) < 0 ||
        semctl(sem_id, SEM_LOG, SETVAL, 1) < 0 ||
        semctl(sem_id, SEM_GATE_START, SETVAL, 0) < 0 ||
        semctl(sem_id, SEM_TAKE_PASSENGERS, SETVAL, 0) < 0 ||
        semctl(sem_id, SEM_LEAVE_PORT, SETVAL, 0) < 0 ||
        semctl(sem_id, SEM_IPC_WAITING_ROOM, SETVAL, 1) < 0 ||
        semctl(sem_id, SEM_MAX_LUGGAGE_SHM, SETVAL, 1) < 0 ||
        semctl(sem_id, SEM_FERRY_START, SETVAL, 0) < 0 ||
        semctl(sem_id, SEM_SHM_PASSENGERS, SETVAL, 1) < 0 ||
        semctl(sem_id, SEM_SHM_GENDER, SETVAL, 1) < 0 ||
        semctl(sem_id, SEM_FERRY_LEFT, SETVAL, 0) < 0)
    {
        perror("Semaphore error");
        exit(-1);
    }

    int ipcKey = ftok(".", 'B');
    if (ipcKey < 0)
    {
        perror("Key error");
        exit(-1);
    }

    int ipcId = msgget(ipcKey, IPC_CREAT | 0600);

    if (ipcId < 0)
    {
        log_error("ORCHESTRATOR", "Failure while creating ipc");
        exit(-1);
    }

    sprintf(strBuff, "%d", ipcId);

    set_env_var(IPC_PASSENGER_QUEUE_ENV, strBuff);

    ipcKey = ftok(".", 'C');
    if (ipcKey < 0)
    {
        perror("Key error");
        exit(-1);
    }

    int ipc_wainting_room = msgget(ipcKey, IPC_CREAT | 0600);

    if (ipc_wainting_room < 0)
    {
        log_error("ORCHESTRATOR", "Failure while creating ipc");
        exit(-1);
    }

    sprintf(strBuff, "%d", ipc_wainting_room);

    set_env_var(WAITING_ROOM_ID, strBuff);

    int shm_key = ftok(".", 'D');
    if (shm_key < 0)
    {
        perror("Key error");
        exit(-1);
    }

    int shm_id = shmget(shm_key, sizeof(int), IPC_CREAT | 0600);
    if (shm_id < 0)
    {
        log_error("ORCHESTRATOR", "Failure while creating shm");
        exit(-1);
    }

    sprintf(strBuff, "%d", shm_id);

    set_env_var(SHM_LUGGAGE_ENV, strBuff);

    shm_key = ftok(".", 'E');
    if (shm_key < 0)
    {
        perror("Key error");
        exit(-1);
    }

    int shm_passengers_id = shmget(shm_key, sizeof(int), IPC_CREAT | 0600);
    if (shm_passengers_id < 0)
    {
        log_error("ORCHESTRATOR", "Failure while creating shm");
        exit(-1);
    }

    sprintf(strBuff, "%d", shm_passengers_id);

    set_env_var(SHM_PASSENGERS_ENV, strBuff);
    passengers_left = shmat(shm_passengers_id, NULL, SHM_RND);
    *passengers_left = PASSENGERS_NUMBER;

    shm_key = ftok(".", 'F');
    if (shm_key < 0)
    {
        perror("Key error");
        exit(-1);
    }

    int shm_gender_id = shmget(shm_key, sizeof(int), IPC_CREAT | 0600);
    if (shm_gender_id < 0)
    {
        log_error("ORCHESTRATOR", "Failure while creating shm");
        exit(-1);
    }

    sprintf(strBuff, "%d", shm_gender_id);

    set_env_var(SHM_GENDER_SWAP_ENV, strBuff);

    int *gender_swap = shmat(shm_gender_id, NULL, SHM_RND);
    *gender_swap = 3;
    shmdt(&gender_swap);

    shm_key = ftok(".", 'G');
    if (shm_key < 0)
    {
        perror("Key error");
        exit(-1);
    }

    int shm_last_gender_id = shmget(shm_key, sizeof(int), IPC_CREAT | 0600);
    if (shm_last_gender_id < 0)
    {
        log_error("ORCHESTRATOR", "Failure while creating shm");
        exit(-1);
    }

    sprintf(strBuff, "%d", shm_last_gender_id);

    set_env_var(SHM_LAST_GENDER_ENV, strBuff);

    available_ferries = init_queue();

    int ferry_ids[FERRY_NUM];
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
        ferry_ids[i] = id;
        enqueue(available_ferries, id);
        custom_sleep(1);
    }

    int passenger_ids[PASSENGERS_NUMBER];
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
        passenger_ids[i] = id;
        custom_sleep(1);
    }

    int gates[GATE_NUM];
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
        gates[i] = id;
    }

    struct sigaction sa;
    sa.sa_sigaction = signal_handler;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    sem_p(SEM_SHM_PASSENGERS);
    while (*passengers_left > 0)
    {
        sprintf(strBuff, "%d passengers left", *passengers_left);
        log_info("ORCHESTRATOR", strBuff);
        sem_v(SEM_SHM_PASSENGERS);

        if (queue_size(available_ferries) < 1)
        {
            pause();
            continue;
        }
        current_ferry = dequeue(available_ferries);
        forced_ferry_leave = false;

        kill(current_ferry, SIGSYS);

        semctl(sem_id, SEM_GATE_START, SETVAL, GATE_NUM);

        custom_sleep_interruptable(FERRY_START_TAKING_PASSENGERS_TIME + FERRY_WAIT_FOR_PASSENGERS_TIME);

        sem_v(SEM_LEAVE_PORT);

        kill(current_ferry, SIGPIPE);

        sem_p(SEM_FERRY_LEFT);

        sem_p(SEM_SHM_PASSENGERS);
    }

    sem_v(SEM_SHM_PASSENGERS);
    if (semctl(sem_id, SEM_FERRY_CAP, SETVAL, GATE_NUM * 3) < 0 ||
        semctl(sem_id, SEM_SHM_PASSENGERS, GATE_NUM * 2) < 0)
    {
        perror("Semaphore error");
        exit(-1);
    }

    free_queue(available_ferries);

    log_info("ORCHESTRATOR", "No passengers left");

    for (int i = 0; i < GATE_NUM; i++)
    {
        kill(gates[i], 9);
    }

    for (int i = 0; i < PASSENGERS_NUMBER; i++)
    {
        kill(passenger_ids[i], 9);
    }

    for (int i = 0; i < FERRY_NUM; i++)
    {
        kill(ferry_ids[i], 9);
    }

    if (shmctl(shm_id, IPC_RMID, NULL) < 0 || shmctl(shm_passengers_id, IPC_RMID, NULL) < 0 || shmctl(shm_gender_id, IPC_RMID, NULL) < 0 || shmctl(shm_last_gender_id, IPC_RMID, NULL) < 0)
    {
        perror("Failed to delete shm");
        exit(-1);
    }

    if (msgctl(ipcId, IPC_RMID, NULL) < 0 || msgctl(ipc_wainting_room, IPC_RMID, NULL) < 0)
    {
        perror("Failed to delete ipc");
        exit(-1);
    }

    if (semctl(sem_id, 0, IPC_RMID) < 0)
    {
        perror("Failed to delete semaphores");
        exit(-1);
    }

    printf("Cleanup ended\n");

    return 0;
}