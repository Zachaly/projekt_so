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
char strBuff[100];
pid_t passenger_ids[PASSENGERS_NUMBER];
pid_t gates[GATE_NUM];
pid_t ferry_ids[FERRY_NUM];
pthread_t passenger_threads[PASSENGERS_NUMBER];
int ipc_passengers_id;
int ipc_wainting_room;
int shm_gender_swap_id;
int shm_passengers_id;
int shm_last_gender_id;
int shm_gender_id;

void cleanup()
{
    free_queue(available_ferries);

    int s;
    // in case that gate is blocked due to one of these semaphores
    if (semctl(sem_id, SEM_FERRY_CAP, SETVAL, GATE_NUM * 3) < 0 ||
        semctl(sem_id, SEM_SHM_PASSENGERS, SETVAL, GATE_NUM * 2) < 0)
    {
        perror("Semaphore error");
        exit(-1);
    }

    for (int i = 0; i < GATE_NUM; i++)
    {
        waitpid(gates[i], &s, 0);
    }

    for (int i = 0; i < PASSENGERS_NUMBER; i++)
    {
        kill(passenger_ids[i], SIGPIPE);
        
        if(pthread_join(passenger_threads[i], NULL) < 0)
        {
            perror("ORCHESTRATOR");
            exit(-1);
        }

        if(pthread_detach(passenger_threads[i]) < 0)
        {
            perror("ORCHESTRATOR");
            exit(-1);
        }
    }

    for (int i = 0; i < FERRY_NUM; i++)
    {
        kill(ferry_ids[i], SIGTERM);
        waitpid(ferry_ids[i], &s, 0);
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

void* wait_passenger(void* _arg)
{
    pid_t pid = *(pid_t *)_arg;
    int s;
    waitpid(pid, &s, 0);
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

    sem_id = semget(semKey, 16, IPC_CREAT | 0600);
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
        semctl(sem_id, SEM_TAKE_PASSENGERS, SETVAL, 0) < 0 ||
        semctl(sem_id, SEM_LEAVE_PORT, SETVAL, 0) < 0 ||
        semctl(sem_id, SEM_IPC_WAITING_ROOM, SETVAL, 1) < 0 ||
        semctl(sem_id, SEM_MAX_LUGGAGE_SHM, SETVAL, 1) < 0 ||
        semctl(sem_id, SEM_FERRY_START, SETVAL, 0) < 0 ||
        semctl(sem_id, SEM_SHM_PASSENGERS, SETVAL, 1) < 0 ||
        semctl(sem_id, SEM_SHM_GENDER, SETVAL, 1) < 0 ||
        semctl(sem_id, SEM_FERRY_LEFT, SETVAL, 0) < 0 ||
        semctl(sem_id, SEM_FERRY_CAN_LEAVE, SETVAL, 0) < 0 ||
        semctl(sem_id, SEM_PEOPLE_AT_GANGWAY, SETVAL, 0) < 0 ||
        semctl(sem_id, SEM_QUEUE_FERRIES, SETVAL, 1) < 0)
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
        enqueue(available_ferries, id);
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

        pthread_t thread;

        if(pthread_create(&thread, NULL, wait_passenger, &id) < 0)
        {
            perror("ORCHESTRATOR");
            cleanup();
            exit(-1);
        }

        passenger_threads[i] = thread;

        custom_sleep(1);
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

    bool last_passengers = false;

    while (*passengers_left > 0)
    {
        if (!last_passengers && *passengers_left < PASSENGERS_QUEUE_SIZE)
        {
            if (semctl(sem_id, SEM_IPC_PASSENGER_QUEUE, SETVAL, PASSENGERS_QUEUE_SIZE) < 0)
            {
                *passengers_left = 0;
                sem_v(SEM_SHM_PASSENGERS);
                cleanup();
                exit(-1);
            }

            last_passengers = true;
        }

        sprintf(strBuff, "%d passengers left", *passengers_left);
        log_info("ORCHESTRATOR", strBuff);
        sem_v(SEM_SHM_PASSENGERS);

        while (queue_size(available_ferries) < 1)
        {
            continue;
        }
        sem_p(SEM_QUEUE_FERRIES);
        current_ferry = dequeue(available_ferries);
        sem_v(SEM_QUEUE_FERRIES);
        forced_ferry_leave = false;

        kill(current_ferry, SIGSYS);

        sem_p(SEM_FERRY_CAN_LEAVE);

        custom_sleep_interruptable(FERRY_START_TAKING_PASSENGERS_TIME);

        sem_v(SEM_TAKE_PASSENGERS);

        custom_sleep_interruptable(FERRY_WAIT_FOR_PASSENGERS_TIME);

        sem_p(SEM_PEOPLE_AT_GANGWAY);

        sem_v(SEM_LEAVE_PORT);

        kill(current_ferry, SIGPIPE);

        sem_p(SEM_FERRY_LEFT);

        sem_p(SEM_SHM_PASSENGERS);
    }

    sem_v(SEM_SHM_PASSENGERS);

    shmdt(&passengers_left);

    custom_sleep(3);

    log_info("ORCHESTRATOR", "No passengers left");

    cleanup();

    return 0;
}