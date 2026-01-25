#include "utilities.h"
#include <sys/wait.h>
#include <signal.h>
#include <stdbool.h>
#include "queue.h"
#include <pthread.h>

Queue *available_ferries;
bool custom_sleep_break = false;
bool forced_ferry_leave = false;
int current_ferry;
int *passengers_left;
char strBuff[200];
pid_t passenger_ids[PASSENGERS_NUMBER];
pid_t gates[GATE_NUM];
pid_t ferry_ids[FERRY_NUM];
int ipc_passengers_id;
int ipc_wainting_room;
int shm_gender_swap_id;
int shm_passengers_id;
int shm_last_gender_id;
int shm_gender_id;
pthread_t passenger_thread;

void cleanup()
{
    free_queue(available_ferries);

    int s;

    for (int i = 0; i < GATE_NUM; i++)
    {
        kill(gates[i], SIGTERM);
    }

    if (semctl(sem_id, SEM_GATE_START, SETVAL, GATE_NUM) < 0 ||
        semctl(sem_id, SEM_FERRY_CAP, SETVAL, GATE_NUM) < 0)
    {
        perror("Semaphore error");
    }

    for (int i = 0; i < GATE_NUM; i++)
    {
        waitpid(gates[i], &s, 0);
    }

    printf("Gates deleted \n");

    for (int i = 0; i < PASSENGERS_NUMBER; i++)
    {
        kill(passenger_ids[i], SIGPIPE);
        waitpid(passenger_ids[i], &s, 0);
    }

    printf("Passengers deleted \n");

    for (int i = 0; i < FERRY_NUM; i++)
    {
        kill(ferry_ids[i], SIGTERM);
        waitpid(ferry_ids[i], &s, 0);
    }

    printf("Ferries deleted\n");

    if (pthread_join(passenger_thread, NULL) < 0)
    {
        perror("ORCHESTRATOR");
        exit(-1);
    }

    if (pthread_detach(passenger_thread) < 0)
    {
        perror("ORCHESTRATOR");
        exit(-1);
    }

    if (shmctl(shm_gender_swap_id, IPC_RMID, NULL) < 0 || shmctl(shm_passengers_id, IPC_RMID, NULL) < 0 || shmctl(shm_gender_id, IPC_RMID, NULL) < 0 || shmctl(shm_last_gender_id, IPC_RMID, NULL) < 0)
    {
        perror("Failed to delete shm");
        exit(-1);
    }

    if (msgctl(ipc_passengers_id, IPC_RMID, NULL) < 0 || msgctl(ipc_wainting_room, IPC_RMID, NULL) < 0)
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
}

void set_env_var(char *name, char *value)
{
    if (setenv(name, value, 1) < 0)
    {
        perror("ORCHESTRATOR");
        exit(-1);
    }
}

void signal_handler(int signum, siginfo_t *info, void *context)
{
    if (signum == SIGTERM)
    {
        pid_t id = info->si_pid;
        sem_p(SEM_QUEUE_FERRIES);
        enqueue(available_ferries, id);
        sem_v(SEM_QUEUE_FERRIES);
        sprintf(strBuff, "Ferry %d returned from course", id);
        log_info("ORCHESTRATOR", strBuff);
    }
    else if (signum == SIGINT)
    {
        log_info("ORCHESTRATOR", "Captain said that ferry should leave");
        if (semctl(sem_id, SEM_FERRY_CAP, SETVAL, 0) < 0)
        {
            perror("ORCHESTRATOR");
            cleanup();
            exit(-1);
        }
        custom_sleep_break = true;
        forced_ferry_leave = true;
    }
}

void custom_sleep_interruptable(int seconds)
{
    time_t start;
    time(&start);
    time_t current;
    time(&current);
    while (current - start < seconds && !custom_sleep_break)
    {
        time(&current);
    }
    custom_sleep_break = false;
}

void *wait_passengers()
{
    int s;

    int start = 0;

    for (int i = 0; i < PASSENGERS_NUMBER; i++)
    {
        waitpid(-1, &s, 0);
    }

    pthread_exit(0);
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

    sem_id = semget(semKey, 14, IPC_CREAT | 0600);
    if (sem_id < 0)
    {
        perror("Error while creating semaphore");
        exit(-1);
    }

    char keyBuff[100];

    sprintf(strBuff, "%d", sem_id);

    set_env_var(SEM_ENV, strBuff);

    if (semctl(sem_id, SEM_IPC_PASSENGER_QUEUE, SETVAL, PASSENGERS_QUEUE_SIZE) < 0 ||
        semctl(sem_id, SEM_LOG, SETVAL, 1) < 0 ||
        semctl(sem_id, SEM_GATE_START, SETVAL, 0) < 0 ||
        semctl(sem_id, SEM_IPC_WAITING_ROOM, SETVAL, 1) < 0 ||
        semctl(sem_id, SEM_MAX_LUGGAGE_SHM, SETVAL, 1) < 0 ||
        semctl(sem_id, SEM_SHM_PASSENGERS, SETVAL, 1) < 0 ||
        semctl(sem_id, SEM_SHM_GENDER, SETVAL, 1) < 0 ||
        semctl(sem_id, SEM_PEOPLE_AT_GANGWAY, SETVAL, 0) < 0 ||
        semctl(sem_id, SEM_QUEUE_FERRIES, SETVAL, 1) < 0 ||
        semctl(sem_id, SEM_PASSENGER_CREATED, SETVAL, 0) < 0 ||
        semctl(sem_id, SEM_FERRY_MOVE_NEXT, SETVAL, 0) < 0 ||
        semctl(sem_id, SEM_ORCHESTRATOR_MOVE_NEXT, SETVAL, 0) < 0 ||
        semctl(sem_id, SEM_WAITING_ROOM_CAP, SETVAL, FERRY_CAPACITY) < 0)
    {
        perror("Semaphore error");
        exit(-1);
    }

    load_sem_id();

    int ipcKey = ftok(".", 'B');
    if (ipcKey < 0)
    {
        perror("Key error");
        exit(-1);
    }

    ipc_passengers_id = msgget(ipcKey, IPC_CREAT | 0600);

    if (ipc_passengers_id < 0)
    {
        perror("ORCHESTRATOR");
        exit(-1);
    }

    sprintf(strBuff, "%d", ipc_passengers_id);

    set_env_var(IPC_PASSENGER_QUEUE_ENV, strBuff);

    ipcKey = ftok(".", 'C');
    if (ipcKey < 0)
    {
        perror("Key error");
        exit(-1);
    }

    ipc_wainting_room = msgget(ipcKey, IPC_CREAT | 0600);

    if (ipc_wainting_room < 0)
    {
        perror("ORCHESTRATOR");
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

    shm_gender_swap_id = shmget(shm_key, sizeof(int), IPC_CREAT | 0600);
    if (shm_gender_swap_id < 0)
    {
        perror("ORCHESTRATOR");
        exit(-1);
    }

    sprintf(strBuff, "%d", shm_gender_swap_id);

    set_env_var(SHM_LUGGAGE_ENV, strBuff);

    shm_key = ftok(".", 'E');
    if (shm_key < 0)
    {
        perror("Key error");
        exit(-1);
    }

    shm_passengers_id = shmget(shm_key, sizeof(int), IPC_CREAT | 0600);
    if (shm_passengers_id < 0)
    {
        perror("ORCHESTRATOR");
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

    shm_gender_id = shmget(shm_key, sizeof(int), IPC_CREAT | 0600);
    if (shm_gender_id < 0)
    {
        perror("ORCHESTRATOR");
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

    shm_last_gender_id = shmget(shm_key, sizeof(int), IPC_CREAT | 0600);
    if (shm_last_gender_id < 0)
    {
        perror("ORCHESTRATOR");
        exit(-1);
    }

    sprintf(strBuff, "%d", shm_last_gender_id);

    set_env_var(SHM_LAST_GENDER_ENV, strBuff);

    available_ferries = init_queue();

    for (int i = 0; i < FERRY_NUM; i++)
    {
        int id = fork();
        switch (id)
        {
        case -1:
            perror("Fork error");
            cleanup();
            exit(-1);
            break;
        case 0:
            execl("ferry.out", "", NULL);
            cleanup();
            break;
        }
        ferry_ids[i] = id;
        sem_p(SEM_QUEUE_FERRIES);
        enqueue(available_ferries, id);
        sem_v(SEM_QUEUE_FERRIES);
        custom_sleep(1);
    }

    for (int i = 0; i < PASSENGERS_NUMBER; i++)
    {
        int id = fork();
        switch (id)
        {
        case -1:
            perror("Fork error");
            cleanup();
            exit(-1);
            break;
        case 0:
            execl("passenger.out", "", NULL);
            cleanup();
            exit(0);
            break;
        }
        passenger_ids[i] = id;

        custom_sleep(1);
    }

    if (pthread_create(&passenger_thread, NULL, wait_passengers, NULL) < 0)
    {
        perror("ORCHESTRATOR");
        cleanup();
        exit(-1);
    }

    for (int i = 0; i < PASSENGERS_NUMBER; i++)
    {
        sem_p(SEM_PASSENGER_CREATED);
    }

    for (int i = 0; i < GATE_NUM; i++)
    {
        int id = fork();
        if (id < 0)
        {
            perror("Fork error");
            cleanup();
            exit(-1);
        }
        else if (id == 0)
        {
            execl("boarding_gate.out", "", NULL);
            cleanup();
            exit(0);
        }
        gates[i] = id;
    }

    struct sigaction sa;
    sa.sa_sigaction = signal_handler;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGPIPE, &sa, NULL);

    while (1)
    {
        sem_p(SEM_SHM_PASSENGERS);
        int pass = *passengers_left;
        sem_v(SEM_SHM_PASSENGERS);

        if (pass < 1)
        {
            break;
        }

        struct msqid_ds queue_data;
        msgctl(ipc_passengers_id, IPC_STAT, &queue_data);

        if (queue_data.msg_qnum < PASSENGERS_QUEUE_SIZE / 5)
        {
            if (semctl(sem_id, SEM_IPC_PASSENGER_QUEUE, SETVAL, PASSENGERS_QUEUE_SIZE) < 0)
            {
                cleanup();
                exit(-1);
            }
        }

        sprintf(strBuff, "%d passengers left", pass);
        log_info("ORCHESTRATOR", strBuff);

        if (queue_size(available_ferries) < 1)
        {
            continue;
        }

        sem_p(SEM_QUEUE_FERRIES);
        current_ferry = dequeue(available_ferries);
        sem_v(SEM_QUEUE_FERRIES);
        forced_ferry_leave = false;

        kill(current_ferry, SIGSYS);

        sem_p(SEM_ORCHESTRATOR_MOVE_NEXT);

        custom_sleep_interruptable(FERRY_START_TAKING_PASSENGERS_TIME);

        sem_v(SEM_FERRY_MOVE_NEXT);

        custom_sleep_interruptable(FERRY_WAIT_FOR_PASSENGERS_TIME);

        sem_p(SEM_PEOPLE_AT_GANGWAY);

        kill(current_ferry, SIGPIPE);

        sem_v(SEM_FERRY_MOVE_NEXT);

        sem_p(SEM_ORCHESTRATOR_MOVE_NEXT);
    }

    sigset_t mask;
    sigemptyset(&mask);

    while (queue_size(available_ferries) < FERRY_NUM)
    {
        sigsuspend(&mask);
    }

    shmdt(&passengers_left);

    custom_sleep(3);

    log_info("ORCHESTRATOR", "No passengers left");

    cleanup();

    return 0;
}