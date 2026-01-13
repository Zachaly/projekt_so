#include "utilities.h"
#include "structures.h"
#include <pthread.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>
#include "queue_pthread.h"

int ipc_id;
int ipc_passengers;
int max_baggage;
bool leave = false;
int *max_baggage_shm;
int shm_id;
int *passenger_left_shm;
int count;
int ferry_passengers;
int last_passengers = 0;
int course_time;
pthread_mutex_t mutex;
bool not_in_port = true;

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

    custom_sleep(5);
    sprintf(buff, "Passenger %d arrived at ferry", passenger.pid);
    log_info("FERRY", buff);

    pthread_mutex_lock(&mutex);
    ferry_passengers++;
    pthread_mutex_unlock(&mutex);

    pthread_exit(0);
}

void go_to_port()
{
    not_in_port = false;
}

void leave_port()
{
    log_info("FERRY", "Ferry will leave the port");

    leave = true;

    sem_v(semId, SEM_FERRY_START);
}

void sig_handler(int signum)
{
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
    ipc_passengers = atoi(getenv(IPC_ENV));

    max_baggage = random_number(MIN_BAGGAGE_LIMIT, MAX_BAGGAGE_LIMIT);

    course_time = random_number(MIN_COURSE_TIME, MAX_COURSE_TIME);

    shm_id = atoi(getenv(SHM_LUGGAGE_ENV));
    int shm_passengers_id = atoi(getenv(SHM_PASSENGERS_ENV));

    char buff[100];

    max_baggage_shm = (int *)shmat(shm_id, NULL, SHM_RND);
    passenger_left_shm = (int *)shmat(shm_passengers_id, NULL, SHM_RND);

    sprintf(buff, "Ferry created with %d cap, %d max baggage nad %d course time", FERRY_CAPACITY, max_baggage, course_time);

    log_info("FERRY", buff);

    signal(SIGSYS, sig_handler);
    signal(SIGPIPE, sig_handler);
    signal(SIGINT, sig_handler);

    sem_p(semId, SEM_SHM_PASSENGERS);
    while (*passenger_left_shm > 0)
    {
        sem_v(semId, SEM_SHM_PASSENGERS);

        while(not_in_port);

        leave = false;
        count = last_passengers;
        ferry_passengers = 0;

        log_info("FERRY", "Ferry in port");

        sem_p(semId, SEM_IPC2);
        struct msqid_ds queue_data;
        msgctl(ipc_id, IPC_STAT, &queue_data);
        sem_v(semId, SEM_IPC2);

        count = queue_data.msg_qnum;

        while (count < FERRY_CAPACITY)
        {
            struct passenger passenger;
            sem_p(semId, SEM_IPC);
            int res = msgrcv(ipc_passengers, &passenger, sizeof(struct passenger) - sizeof(long int), 3, IPC_NOWAIT);
            sem_v(semId, SEM_IPC);
            if (res < 0)
            {
                break;
            }

            sprintf(buff, "Passenger %d skipped queue and added to waiting room as VIP", passenger.pid);
            log_info("FERRY", buff);

            sem_p(semId, SEM_IPC2);
            if (msgsnd(ipc_id, (struct passenger *)&passenger, sizeof(struct passenger) - sizeof(long int), 0) < 0)
            {
                perror("Ipc fail");
                exit(-1);
            }
            sem_v(semId, SEM_IPC2);
            custom_sleep(1);
            count++;
        }

        if (semctl(semId, SEM_FERRY_CAP, SETVAL, FERRY_CAPACITY - count) < 0)
        {
            log_error("FERRY", "FERRY");
            exit(-1);
        }

        sem_p(semId, SEM_MAX_LUGGAGE_SHM);
        *max_baggage_shm = max_baggage;
        sem_v(semId, SEM_MAX_LUGGAGE_SHM);

        custom_sleep(FERRY_START_TAKING_PASSENGERS_TIME);

        log_info("FERRY", "Ferry started taking passengers");

        Queue *thread_ids = init_queue();

        while (!leave)
        {
            if (queue_size(thread_ids) == GANGWAY_CAP)
            {
                pthread_t id = dequeue(thread_ids);

                pthread_join(id, NULL);
                pthread_detach(id);
            }

            pthread_t thread_id;

            pthread_create(&thread_id, NULL, *gangway, NULL);

            enqueue(thread_ids, thread_id);

            custom_sleep(2);
        }

        while (queue_size(thread_ids) > 0)
        {
            pthread_t id = dequeue(thread_ids);

            int j_res = pthread_join(id, NULL);
            if (j_res != 0)
            {
                continue;
            }

            pthread_detach(id);
        }

        free_queue(thread_ids);

        sem_p(semId, SEM_LEAVE_PORT);

        if (ferry_passengers == 0)
        {
            log_info("FERRY", "Ferry didn't take off due to no passengers");
            sem_v(semId, SEM_FERRY_LEFT);
            sem_p(semId, SEM_FERRY_START);
            sem_p(semId, SEM_SHM_PASSENGERS);
            continue;
        }

        char buff[100];
        sprintf(buff, "Ferry started course");
        not_in_port = true;
        sem_p(semId, SEM_FERRY_START);
        log_info("FERRY", buff);
        sem_v(semId, SEM_FERRY_LEFT);

        custom_sleep(course_time);

        sem_p(semId, SEM_SHM_PASSENGERS);
        *passenger_left_shm -= ferry_passengers;
        last_passengers = FERRY_CAPACITY - ferry_passengers;
        sem_v(semId, SEM_SHM_PASSENGERS);

        kill(getppid(), SIGTERM);

        sem_p(semId, SEM_SHM_PASSENGERS);
    }

    sem_v(semId, SEM_SHM_PASSENGERS);

    shmdt(&max_baggage_shm);
    shmdt(&passenger_left_shm);

    return 0;
}