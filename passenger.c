#include "utilities.h"
#include "structures.h"
#include <signal.h>

#define MIN_LUGGAGE 0
#define MAX_LUGGAGE 20

int random_number(int min, int max);

struct passenger data;
int ipc_id;
int *max_luggage;

void readd_to_queue(int sig)
{
    if (sig == SIGTERM)
    {
        sem_p(semId, SEM_MAX_LUGGAGE_SHM);
        data.baggage = *max_luggage;
        sem_v(semId, SEM_MAX_LUGGAGE_SHM);

        sem_p(semId, SEM_IPC);

        if (msgsnd(ipc_id, (struct passenger *)&data, sizeof(struct passenger) - sizeof(long int), 0) < 0)
        {
            log_error("PASSENGER", "Failure while adding passenger to queue");
            exit(-1);
        }
        sem_v(semId, SEM_IPC);

        log_info("PASSENGER", "Passenger adjusted the baggage");
    }
}

int main()
{
    load_sem_id();
    srand(time(NULL));

    data.baggage = random_number(MIN_LUGGAGE, MAX_LUGGAGE);
    data.wait_limit = 3;
    data.mtype = random_number(0, 1000) >= 500 ? MALE : FEMALE;
    data.pid = getpid();

    char buff[200];

    int shm_id = atoi(getenv(SHM_LUGGAGE_ENV));
    max_luggage = shmat(shm_id, NULL, SHM_RDONLY);

    sprintf(buff, "%ld passenger created with %d baggage", data.mtype, data.baggage);

    log_info("PASSENGER", buff);

    char *idStr = getenv(IPC_ENV);

    ipc_id = atoi(idStr);

    sem_p(semId, SEM_IPC);

    if (msgsnd(ipc_id, (struct passenger *)&data, sizeof(struct passenger) - sizeof(long int), 0) < 0)
    {
        log_error("PASSENGER", "Failure while adding passenger to queue");
        exit(-1);
    }

    signal(SIGTERM, readd_to_queue);

    sem_v(semId, SEM_IPC);

    sem_p(semId, SEM_END);
    shmdt(max_luggage);

    return 0;
}