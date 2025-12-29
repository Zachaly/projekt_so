#include "utilities.h"

#include "structures.h"

#define MIN_LUGGAGE 0
#define MAX_LUGGAGE 20

int random_number(int min, int max);

int main()
{
    srand(time(NULL));

    struct passenger data;

    data.baggage = random_number(MIN_LUGGAGE, MAX_LUGGAGE);
    data.gender = random_number(FEMALE, MALE + 1);
    data.wait_limit = 3;

    char buff[200];

    sprintf(buff, "%d passenger created with %d baggage", data.gender, data.baggage);

    log_info("PASSENGER", buff);

    return 0;
}

int random_number(int min, int max)
{
    return (rand() % max) + min;
}