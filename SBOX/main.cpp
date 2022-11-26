#include <stdlib.h>
#include <time.h>
//#include <iostream>
using namespace std;
#define MIN 0
#define MAX 99

void work_parse(uint badval, uint &balance, uint &cost){
    if (badval < balance){
        balance -= badval;
        cost += badval;
    }
}

int main()
{
    uint balance = 100;
    uint cost = 100;
    srand((unsigned)time(NULL));
    uint i = rand() % (MAX + MIN - 1);
    work_parse(i, balance, cost);
    //printf("%d",balance);
    return 0;
}