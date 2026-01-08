#include "utilities.h"
#include <sys/wait.h>

#define PASSENGERS_NUMBER 20
#define GATE_NUM 3

void set_env_var(char *name, char *value)
{
    if (setenv(name, value, 1) < 0)
    {
        log_error("ORCHESTRATOR", "Failure while creating ipc");
        exit(-1);
    }
}

int main()
{
    char log_file_buff[100];

    time_t now = time(NULL);
    struct tm *local_time = localtime(&now);

    strftime(log_file_buff, sizeof(log_file_buff), "LOGS_%Y_%m_%d_%H_%M_%S.txt", local_time);

    if (setenv(LOG_ENV, log_file_buff, 1) < 0)
    {
        perror("Problem with setting env");
        exit(-1);
    }

    int semKey = ftok(".", 'A');

    int semId;

    if ((semId = semget(semKey, 3, IPC_CREAT | 0660)) < 0)
    {
        perror("Error while creating semaphore");
        exit(-1);
    }

    char keyBuff[100];

    sprintf(keyBuff, "%d", semId);

    set_env_var(SEM_ENV, keyBuff);

    semctl(semId, SEM_IPC, SETVAL, 1);
    semctl(semId, SEM_LOG, SETVAL, 1);
    load_sem_id();

    int ipcKey = ftok(".", 'B');

    int ipcId;

    if ((ipcId = msgget(ipcKey, IPC_CREAT | 0600)) < 0)
    {
        log_error("ORCHESTRATOR", "Failure while creating ipc");
        exit(-1);
    }

    sprintf(keyBuff, "%d", ipcId);

    set_env_var(IPC_ENV, keyBuff);

    for (int i = 0; i < PASSENGERS_NUMBER; i++)
    {
        int id = fork();
        switch (id)
        {
        case -1:
            perror("Fork error");
            exit(-1);
            break;
        case 0:
            execl("passenger.out", "", NULL);
            exit(0);
            break;
        }
        sleep(1);
    }

    for (int i = 0; i < 3; i++)
    {
        int id = fork();
        if (id < 0)
        {
            perror("Fork error");
            exit(-1);
        }
        else if (id == 0)
        {
            execl("boarding_gate.out", "", NULL);
            exit(0);
        }
    }

    wait(NULL);

    if (msgctl(ipcId, IPC_RMID, NULL) < 0)
    {
        log_error("ORCHESTRATOR", "Failed to delete ipc");
        exit(-1);
    }

    log_info("ORCHESTRATOR", "IPC closed");

    if (semctl(semId, 0, IPC_RMID) < 0)
    {
        perror("Failed to delete semaphore");
        exit(-1);
    }

    log_info("ORCHESTRATOR", "Semaphore closed");

    return 0;
}