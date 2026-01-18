#include "utilities.h"
#include "structures.h"
#include <pthread.h>
#include <stdbool.h>
#include "errno.h"
#include "queue_pthread.h"

int current_gender = 0;
int ipc_passenger_queue_id;
int stop = false;
pthread_mutex_t mutex;
int ipc_waiting_room;
int *shm_max_luggage;
int *shm_gender_swap;
int *shm_last_gender;
Queue *gate_passengers;

pthread_t id_1;
pthread_t id_2;

void sig_handler(int signum) {}

void *take_passenger()
{
    sem_p(SEM_FERRY_CAP);

    struct passenger passenger;

    int result = msgrcv(ipc_passenger_queue_id, &passenger, sizeof(struct passenger) - sizeof(long int), current_gender, IPC_NOWAIT);

    pthread_mutex_lock(&mutex);
    if (errno == ENOMSG && current_gender == 0)
    {
        sem_v(SEM_FERRY_CAP);
        pthread_mutex_unlock(&mutex);
        pthread_exit(0);
        return NULL;
    }
    else if (errno == ENOMSG)
    {
        sem_v(SEM_FERRY_CAP);
        current_gender = 0;
        pthread_mutex_unlock(&mutex);
        pthread_exit(0);
        return NULL;
    }
    else if (result < 0)
    {
        pthread_mutex_unlock(&mutex);
        sem_v(SEM_FERRY_CAP);
        perror("GATE");
        pthread_exit(0);
        return NULL;
    }

    sem_p(SEM_SHM_GENDER);
    if (*shm_last_gender == passenger.mtype)
    {
        *shm_gender_swap -= 1;
    }
    else
    {
        *shm_gender_swap = 3;
    }
    if (current_gender != 0 && current_gender != passenger.mtype && queue_size(gate_passengers) > 0)
    {
        pthread_t id = dequeue(gate_passengers);
        pthread_join(id, NULL);
        pthread_detach(id);
    }

    *shm_last_gender = passenger.mtype;
    sem_v(SEM_SHM_GENDER);

    current_gender = passenger.mtype;
    pthread_mutex_unlock(&mutex);

    char log_buff[200];
    sprintf(log_buff, "Passenger(%d) with gender %ld and baggage %d arrived at the gate", passenger.pid,
            passenger.mtype, passenger.baggage);

    log_info("GATE", log_buff);

    custom_sleep(5);

    sem_p(SEM_IPC_WAITING_ROOM);
    sem_p(SEM_MAX_LUGGAGE_SHM);

    if (passenger.baggage > *shm_max_luggage)
    {
        sprintf(log_buff, "Passenger %d exceeded max baggage %d/%d", passenger.pid, passenger.baggage, *shm_max_luggage);
        log_info("GATE", log_buff);
        kill(passenger.pid, SIGTERM);
        sem_v(SEM_FERRY_CAP);
    }
    else
    {
        sprintf(log_buff, "Passenger %d left the gate", passenger.pid);

        log_info("GATE", log_buff);
        if (msgsnd(ipc_waiting_room, &passenger, sizeof(struct passenger) - sizeof(long int), 0) < 0)
        {
            perror("GATE");
            exit(-1);
        }
        sem_v(SEM_IPC_PASSENGER_QUEUE);
    }
    sem_v(SEM_IPC_WAITING_ROOM);
    sem_v(SEM_MAX_LUGGAGE_SHM);

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

    shm_max_luggage = (int *)shmat(shm_id, NULL, SHM_RDONLY);
    int *passengers = (int *)shmat(shm_passengers_id, NULL, SHM_RDONLY);
    shm_gender_swap = (int *)shmat(shm_gender_swap_id, NULL, SHM_RND);
    shm_last_gender = (int *)shmat(shm_last_gender_id, NULL, SHM_RND);

    ipc_passenger_queue_id = atoi(getenv(IPC_PASSENGER_QUEUE_ENV));
    ipc_waiting_room = atoi(getenv(WAITING_ROOM_ID));

    pthread_mutex_init(&mutex, NULL);

    gate_passengers = init_queue();
    sem_p(SEM_GATE_START);
    log_info("GATE", "Gate opened");

    sem_p(SEM_SHM_PASSENGERS);
    while (*passengers > 0)
    {
        sem_v(SEM_SHM_PASSENGERS);

        sem_p(SEM_SHM_GENDER);
        if (*shm_gender_swap < 0)
        {
            *shm_gender_swap = 3;
            sem_v(SEM_SHM_GENDER);

            current_gender = 0;

            log_info("GATE", "Gate forced to change current gender");

            while (queue_size(gate_passengers) > 0)
            {
                pthread_t id = dequeue(gate_passengers);
                pthread_join(id, NULL);
                pthread_detach(id);
            }
            sem_p(SEM_SHM_PASSENGERS);
            continue;
        }
        sem_v(SEM_SHM_GENDER);

        if (queue_size(gate_passengers) == 0)
        {
            current_gender = 0;
        }

        while (queue_size(gate_passengers) > 1)
        {
            pthread_t id = dequeue(gate_passengers);
            pthread_join(id, NULL);
            pthread_detach(id);
        }

        pthread_t id;
        if (pthread_create(&id, NULL, *take_passenger, NULL))
        {
            perror("GATE");
            exit(-1);
        }
        enqueue(gate_passengers, id);

        custom_sleep(1);
        sem_p(SEM_SHM_PASSENGERS);
    }
    sem_v(SEM_SHM_PASSENGERS);

    free(gate_passengers);
    log_info("GATE", "Gate closed");

    shmdt(&shm_max_luggage);
    shmdt(&passengers);
    shmdt(&shm_gender_swap);
    shmdt(&shm_last_gender);

    return 0;
}