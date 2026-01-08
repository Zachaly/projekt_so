#include "utilities.h"
#include "structures.h"
#include <pthread.h>
#include <stdbool.h>
#include <semaphore.h>

#define MAX_LUGGAGE 20

int current_gender = 0;
int ipcId;
int stop = false;
pthread_mutex_t mutex_passenger_num;
pthread_mutex_t mutex_gender;
int passenger_num = 0;

sem_t thread_sem;

void *take_passenger()
{
    struct passenger passenger;

    pthread_mutex_lock(&mutex_gender);
    sem_p(semId, SEM_IPC);
    int result = msgrcv(ipcId, &passenger, sizeof(struct passenger) - sizeof(long int), current_gender, IPC_NOWAIT);
    sem_v(semId, SEM_IPC);

    if (errno == ENOMSG && current_gender == 0)
    {
        pthread_mutex_lock(&mutex_passenger_num);
        passenger_num--;
        pthread_mutex_unlock(&mutex_passenger_num);
        log_info("GATE", "No passengers left");
        stop = true;
        pthread_exit(0);
        return NULL;
    }
    else if (errno == ENOMSG)
    {
        pthread_mutex_lock(&mutex_passenger_num);
        passenger_num--;
        pthread_mutex_unlock(&mutex_passenger_num);

        pthread_exit(0);
        return NULL;
    }

    if (result < 0)
    {
        pthread_mutex_lock(&mutex_passenger_num);
        passenger_num--;
        pthread_mutex_unlock(&mutex_passenger_num);
        log_error("GATE", "IPC error");
        stop = true;
        pthread_exit(0);
        return NULL;
    }

    current_gender = passenger.mtype;
    pthread_mutex_unlock(&mutex_gender);

    char log_buff[200];
    sprintf(log_buff, "Passenger(%d) with gender %ld and baggage %d arrived at the gate", passenger.pid,
            passenger.mtype, passenger.baggage);

    log_info("GATE", log_buff);

    sleep(5);

    sprintf(log_buff, "Passenger(%d) left the gate", passenger.pid);
    log_info("GATE", log_buff);

    pthread_mutex_lock(&mutex_passenger_num);
    passenger_num--;
    pthread_mutex_unlock(&mutex_passenger_num);

    pthread_exit(0);
}

int main()
{
    load_sem_id();

    log_info("GATE", "Gate opened");

    char *ipcIdStr = getenv(IPC_ENV);

    ipcId = atoi(ipcIdStr);

    if (sem_init(&thread_sem, 0, 1) < 0)
    {
        log_error("GATE", "Sem error");
        exit(-1);
    }

    pthread_mutex_init(&mutex_passenger_num, NULL);
    pthread_mutex_init(&mutex_gender, NULL);

    pthread_t id_1;
    pthread_t id_2;

    while (!stop)
    {
        if (passenger_num == 0)
        {
            current_gender = 0;
        }

        if (passenger_num >= 2)
        {
            if (pthread_join(id_1, NULL) < 0 || pthread_join(id_2, NULL) < 0)
            {
                log_error("GATE", "Error while joining thread");
                exit(-1);
            }

            if (pthread_detach(id_1) < 0 || pthread_detach(id_2) < 0)
            {
                log_error("GATE", "Error while detaching thread");
                exit(-1);
            }
            continue;
        }

        if (passenger_num > 0)
        {
            passenger_num++;
            if (pthread_create(&id_2, NULL, *take_passenger, NULL))
            {
                log_error("GATE", "Error while creating thread");
                exit(-1);
            }
            continue;
        }

        passenger_num++;
        if (pthread_create(&id_1, NULL, *take_passenger, NULL) < 0)
        {
            log_error("GATE", "Error while creating thread");
            exit(-1);
        }
    }

    log_info("GATE", "No passengers left");

    return 0;
}