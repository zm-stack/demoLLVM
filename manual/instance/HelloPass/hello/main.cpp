#include <iostream>
#include <stdlib.h>
#include <time.h>
using namespace std;
#define MIN 0
#define MAX 99

int main()
{
    srand((unsigned)time(NULL));
    int i = rand() % (MAX + MIN - 1);
    if (i < 50) {
        i += 100;
    }else{
        i += 200;
    }
    printf("%d", i);
    return 0;
}