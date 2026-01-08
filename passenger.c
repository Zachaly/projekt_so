#include "utilities.h"

#include "structures.h"

#define MIN_LUGGAGE 0
#define MAX_LUGGAGE 20

int random_number(int min, int max);

int main()
{
    load_sem_id();
    srand(time(NULL));

    struct passenger data;

    data.baggage = random_number(MIN_LUGGAGE, MAX_LUGGAGE);
    data.wait_limit = 3;
    data.mtype = random_number(0, 1000) >= 500 ? MALE : FEMALE;
    data.pid = getpid();

    char buff[200];

    sprintf(buff, "%ld passenger created with %d baggage", data.mtype, data.baggage);

    log_info("PASSENGER", buff);

    char* idStr = getenv(IPC_ENV);

    int ipcId = atoi(idStr);

    sem_p(semId, SEM_IPC);

    if(msgsnd(ipcId, (struct passenger*)&data, sizeof(struct passenger) - sizeof(long int), 0) < 0)
    {
        log_error("PASSENGER", "Failure while adding passenger to queue");
        exit(-1);
    }

    sem_v(semId, SEM_IPC);

    sleep(60);

    return 0;
}

int random_number(int min, int max)
{
    return (rand() % max) + min;
}