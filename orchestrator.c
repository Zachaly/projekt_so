#include "utilities.h"

#define PASSENGERS_NUMBER 10

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

    for (int i = 0; i < PASSENGERS_NUMBER; i++)
    {
        int id = fork();
        sleep(1);
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
    }

    wait(NULL);

    return 0;
}