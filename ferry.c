#include "utilities.h"
#include "structures.h"
#include <pthread.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>

#define MIN_BAGGAGE_LIMIT 10
#define MAX_BAGGAGE_LIMIT 15

int ipc_id;
int max_baggage;
bool leave = false;
int *max_baggage_shm;
int shm_id;

void *gangway()
{
    char buff[100];

    struct passenger passenger;
    sem_p(semId, SEM_IPC2);
    int res = msgrcv(ipc_id, &passenger, sizeof(struct passenger) - sizeof(long int), 0, IPC_NOWAIT);
    sem_v(semId, SEM_IPC2);
    if (res < 0 && errno == ENOMSG)
    {
        pthread_exit(0);
        return NULL;
    }
    else if (res < 0)
    {
        pthread_exit(0);
        return NULL;
    }

    sprintf(buff, "Passenger %d arrived at gangway", passenger.pid);

    log_info("FERRY", buff);

    sleep(5);
    sprintf(buff, "Passenger %d arrived at ferry", passenger.pid);
    log_info("FERRY", buff);

    pthread_exit(0);
}

void go_to_port()
{
    leave = false;

    log_info("FERRY", "Ferry in port");
    if (semctl(semId, SEM_FERRY_CAP, SETVAL, FERRY_CAPACITY) < 0)
    {
        log_error("FERRY", "Failed to set semaphore");
        exit(-1);
    }

    sem_p(semId, SEM_MAX_LUGGAGE_SHM);
    *max_baggage_shm = max_baggage;
    sem_v(semId, SEM_MAX_LUGGAGE_SHM);
}

void take_passengers()
{
    int count = 0;

    log_info("FERRY", "Ferry started taking passengers");

    pthread_t thread_ids[GANGWAY_CAP];
    int i = 0;

    bool append_i = true;

    while (count < FERRY_CAPACITY && !leave)
    {
        if (i == GANGWAY_CAP)
        {
            pthread_join(thread_ids[0], NULL);
            pthread_detach(thread_ids[0]);
            i = 0;
            append_i = false;
        }

        pthread_create(&thread_ids[i], NULL, *gangway, NULL);

        if (append_i)
        {
            i++;
        }
        count++;
        sleep(2);
    }

    for (int i = 0; i < GANGWAY_CAP; i++)
    {
        int j_res = pthread_join(thread_ids[0], NULL);
        if (j_res < 0 && errno == ESRCH)
        {
            continue;
        }

        pthread_detach(thread_ids[i]);
    }

    sem_p(semId, SEM_LEAVE_PORT);

    log_info("FERRY", "Ferry will leave the port");

    sem_p(semId, SEM_FERRY_START);
    log_info("FERRY", "Ferry started course");

    sleep(30);

    kill(getppid(), SIGTERM);
}

void leave_port()
{
    leave = true;
    sem_v(semId, SEM_FERRY_START);
}

void sig_handler(int signum)
{
    if (signum == SIGTERM)
    {
        take_passengers();
    }
    if (signum == SIGSYS)
    {
        go_to_port();
    }
    if (signum == SIGPIPE)
    {
        leave_port();
    }
}

int main()
{
    load_sem_id();

    srand(time(NULL));

    ipc_id = atoi(getenv(WAITING_ROOM_ID));

    max_baggage = random_number(MIN_BAGGAGE_LIMIT, MAX_BAGGAGE_LIMIT);

    shm_id = atoi(getenv(SHM_LUGGAGE_ENV));

    char buff[100];

    max_baggage_shm = (int *)shmat(shm_id, NULL, SHM_RND);

    sprintf(buff, "Ferry created with %d cap and %d max baggage", FERRY_CAPACITY, max_baggage);

    log_info("FERRY", buff);

    signal(SIGTERM, sig_handler);
    signal(SIGSYS, sig_handler);
    signal(SIGPIPE, sig_handler);

    sem_p(semId, SEM_END);
    sem_v(semId, SEM_END);

    shmdt(&max_baggage_shm);

    return 0;
}