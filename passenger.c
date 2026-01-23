#include "utilities.h"
#include "structures.h"
#include <signal.h>
#include <stdbool.h>
#include <sys/wait.h>

int random_number(int min, int max);

struct passenger data;
int ipc_id;
int *shm_max_luggage;
int *passengers;
bool live = true;

void readd_to_queue(int sig)
{
    if (sig == SIGTERM)
    {
        sem_p(SEM_MAX_LUGGAGE_SHM);
        data.baggage = *shm_max_luggage;
        sem_v(SEM_MAX_LUGGAGE_SHM);

        if (msgsnd(ipc_id, (struct passenger *)&data, sizeof(struct passenger) - sizeof(long int), 0) < 0)
        {
            perror("PASSENGER");
            exit(-1);
        }

        log_info("PASSENGER", "Passenger adjusted the baggage");
    }
    else if (sig == SIGPIPE)
    {
        live = false;
    }
}

int main()
{
    load_sem_id();
    signal(SIGINT, readd_to_queue);
    signal(SIGTERM, readd_to_queue);

    srand(time(NULL));

    data.baggage = random_number(MIN_PASSENGER_LUGGAGE, MAX_PASSENGER_LUGGAGE);
    data.mtype = random_number(0, 1000) >= MALE_CHANCE ? MALE : FEMALE;
    data.pid = getpid();

    bool is_vip = random_number(0, 1000) >= VIP_CHANCE ? false : true;

    if (is_vip)
    {
        data.mtype = 3;
    }

    char buff[200];

    int shm_luggage_id = atoi(getenv(SHM_LUGGAGE_ENV));
    shm_max_luggage = shmat(shm_luggage_id, NULL, SHM_RDONLY);

    int shm_passengers_id = atoi(getenv(SHM_PASSENGERS_ENV));
    passengers = (int *)shmat(shm_passengers_id, NULL, SHM_RND);

    char *idStr = getenv(IPC_PASSENGER_QUEUE_ENV);

    ipc_id = atoi(idStr);

    sem_p(SEM_IPC_PASSENGER_QUEUE);
    if (msgsnd(ipc_id, (struct passenger *)&data, sizeof(struct passenger) - sizeof(long int), 0) < 0)
    {
        perror("PASSENGER");
        exit(-1);
    }

    sprintf(buff, "%ld passenger created with %d baggage(is VIP? %d)", data.mtype, data.baggage, is_vip);

    log_info("PASSENGER", buff);

    sigset_t mask;
    sigemptyset(&mask);

    while (live)
    {
        sigsuspend(&mask);
    }

    shmdt(&passengers);
    shmdt(&shm_max_luggage);

    return 0;
}