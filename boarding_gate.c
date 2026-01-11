#include "utilities.h"
#include "structures.h"
#include <pthread.h>
#include <stdbool.h>
#include "errno.h"

int current_gender = 0;
int ipcId;
int stop = false;
pthread_mutex_t mutex;
int passenger_num = 0;
int ipc_waiting_room;
int *max_luggage;
int *shm_gender_swap;
int *shm_last_gender;

pthread_t id_1;
pthread_t id_2;

void sig_handler(int signum) {}

void *take_passenger()
{
    sem_p(semId, SEM_FERRY_CAP);

    struct passenger passenger;

    sem_p(semId, SEM_IPC);
    int result = msgrcv(ipcId, &passenger, sizeof(struct passenger) - sizeof(long int), current_gender, IPC_NOWAIT);
    sem_v(semId, SEM_IPC);

    pthread_mutex_lock(&mutex);
    if (errno == ENOMSG && current_gender == 0)
    {
        passenger_num--;
        sem_v(semId, SEM_FERRY_CAP);
        pthread_mutex_unlock(&mutex);
        pthread_exit(0);
        return NULL;
    }
    else if (errno == ENOMSG)
    {
        passenger_num--;
        sem_v(semId, SEM_FERRY_CAP);
        current_gender = 0;
        pthread_mutex_unlock(&mutex);
        pthread_exit(0);
        return NULL;
    }
    else if (result < 0)
    {
        passenger_num--;
        pthread_mutex_unlock(&mutex);
        sem_v(semId, SEM_FERRY_CAP);
        log_error("GATE", "IPC error");
        pthread_exit(0);
        return NULL;
    }

    sem_p(semId, SEM_SHM_GENDER);
    if (*shm_last_gender == passenger.mtype)
    {
        *shm_gender_swap -= 1;
    }
    else
    {
        *shm_gender_swap = 3;
    }
    *shm_last_gender = passenger.mtype;
    sem_v(semId, SEM_SHM_GENDER);

    current_gender = passenger.mtype;
    pthread_mutex_unlock(&mutex);

    char log_buff[200];
    sprintf(log_buff, "Passenger(%d) with gender %ld and baggage %d arrived at the gate", passenger.pid,
            passenger.mtype, passenger.baggage);

    log_info("GATE", log_buff);

    sleep(5);

    pthread_mutex_lock(&mutex);
    sem_p(semId, SEM_MAX_LUGGAGE_SHM);

    if (passenger.baggage > *max_luggage)
    {
        sprintf(log_buff, "Passenger %d exceeded max baggage %d/%d", passenger.pid, passenger.baggage, *max_luggage);
        log_info("GATE", log_buff);
        kill(passenger.pid, SIGTERM);
        sem_v(semId, SEM_FERRY_CAP);
    }
    else
    {
        sprintf(log_buff, "Passenger %d left the gate", passenger.pid);

        log_info("GATE", log_buff);
        sem_p(semId, SEM_IPC2);
        if (msgsnd(ipc_waiting_room, &passenger, sizeof(struct passenger) - sizeof(long int), 0) < 0)
        {
            log_error("GATE", "Failure while writing to ipc");
            exit(-1);
        }
        sem_v(semId, SEM_IPC2);
    }
    sem_v(semId, SEM_MAX_LUGGAGE_SHM);

    passenger_num--;
    pthread_mutex_unlock(&mutex);

    pthread_exit(0);
}

int main()
{
    load_sem_id();

    signal(SIGINT, sig_handler);

    int shm_id = atoi(getenv(SHM_LUGGAGE_ENV));
    int shm_passengers_id = atoi(getenv(SHM_PASSENGERS_ENV));
    int shm_gender_swap_id = atoi(getenv(SHM_GENDER_SWAP_ENV));
    int shm_last_gender_id = atoi(getenv(SHM_LAST_GENDER_ENV));

    max_luggage = (int *)shmat(shm_id, NULL, SHM_RDONLY);
    int *passengers = (int *)shmat(shm_passengers_id, NULL, SHM_RDONLY);
    shm_gender_swap = (int *)shmat(shm_gender_swap_id, NULL, SHM_RND);
    shm_last_gender = (int *)shmat(shm_last_gender_id, NULL, SHM_RND);

    char *ipcIdStr = getenv(IPC_ENV);

    ipcId = atoi(ipcIdStr);
    ipc_waiting_room = atoi(getenv(WAITING_ROOM_ID));

    pthread_mutex_init(&mutex, NULL);

    sem_p(semId, SEM_GATE_START);
    log_info("GATE", "Gate opened");

    sem_p(semId, SEM_SHM_PASSENGERS);
    while (*passengers > 0)
    {
        sem_v(semId, SEM_SHM_PASSENGERS);

        sem_p(semId, SEM_SHM_GENDER);
        if (*shm_gender_swap < 0)
        {
            *shm_gender_swap = 3;
            sem_v(semId, SEM_SHM_GENDER);

            current_gender = current_gender == MALE ? FEMALE : MALE;

            log_info("GATE", "Gate forced to change current gender");

            if (pthread_join(id_1, NULL) < 0 || (passenger_num > 1 && pthread_join(id_2, NULL) < 0))
            {
                log_error("GATE", "Error while joining thread");
                exit(-1);
            }

            if (pthread_detach(id_1) < 0 || (passenger_num > 1 & pthread_detach(id_2) < 0))
            {
                log_error("GATE", "Error while detaching thread");
                exit(-1);
            }
            passenger_num = 0;
            sem_p(semId, SEM_SHM_PASSENGERS);
            continue;
        }
        sem_v(semId, SEM_SHM_GENDER);

        if (passenger_num == 0)
        {
            current_gender = 0;
        }

        if (passenger_num > 1)
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
            sem_p(semId, SEM_SHM_PASSENGERS);
            continue;
        }

        sleep(1);

        if (passenger_num > 0)
        {
            passenger_num++;
            if (pthread_create(&id_2, NULL, *take_passenger, NULL))
            {
                log_error("GATE", "Error while creating thread");
                exit(-1);
            }
            sem_p(semId, SEM_SHM_PASSENGERS);
            continue;
        }

        passenger_num++;
        if (pthread_create(&id_1, NULL, *take_passenger, NULL) < 0)
        {
            log_error("GATE", "Error while creating thread");
            exit(-1);
        }

        sleep(1);
        sem_p(semId, SEM_SHM_PASSENGERS);
    }
    sem_v(semId, SEM_SHM_PASSENGERS);
    sem_p(semId, SEM_END);

    shmdt(&max_luggage);
    shmdt(&passengers);
    shmdt(&shm_gender_swap);

    return 0;
}