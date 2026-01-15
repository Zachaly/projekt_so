#include "utilities.h"
#include "structures.h"
#include <signal.h>
#include <stdbool.h>

int random_number(int min, int max);

struct passenger data;
int ipc_id;
int *shm_max_luggage;

void readd_to_queue(int sig)
{
    if (sig == SIGTERM)
    {
        sem_p(SEM_MAX_LUGGAGE_SHM);
        data.baggage = *shm_max_luggage;
        sem_v(SEM_MAX_LUGGAGE_SHM);

        sem_p(SEM_IPC_PASSENGER_QUEUE);
        if (msgsnd(ipc_id, (struct passenger *)&data, sizeof(struct passenger) - sizeof(long int), 0) < 0)
        {
            log_error("PASSENGER", "Failure while adding passenger to queue");
            exit(-1);
        }
        sem_v(SEM_IPC_PASSENGER_QUEUE);

        log_info("PASSENGER", "Passenger adjusted the baggage");
    }
}

int main()
{
    load_sem_id();
    srand(time(NULL));
    signal(SIGINT, readd_to_queue);

    data.baggage = random_number(MIN_PASSENGER_LUGGAGE, MAX_PASSENGER_LUGGAGE);
    data.mtype = random_number(0, 1000) >= MALE_CHANCE ? MALE : FEMALE;
    data.pid = getpid();

    bool is_vip = random_number(0, 1000) >= VIP_CHANCE ? false : true;

    if(is_vip)
    {
        data.mtype = 3;
    }

    char buff[200];

    int shm_id = atoi(getenv(SHM_LUGGAGE_ENV));
    shm_max_luggage = shmat(shm_id, NULL, SHM_RDONLY);

    sprintf(buff, "%ld passenger created with %d baggage(is VIP? %d)", data.mtype, data.baggage, is_vip);

    log_info("PASSENGER", buff);

    char *idStr = getenv(IPC_PASSENGER_QUEUE_ENV);

    ipc_id = atoi(idStr);

    sem_p(SEM_IPC_PASSENGER_QUEUE);

    if (msgsnd(ipc_id, (struct passenger *)&data, sizeof(struct passenger) - sizeof(long int), 0) < 0)
    {
        log_error("PASSENGER", "Failure while adding passenger to queue");
        exit(-1);
    }

    signal(SIGTERM, readd_to_queue);

    sem_v(SEM_IPC_PASSENGER_QUEUE);

    while(true);

    shmdt(&shm_max_luggage);

    return 0;
}