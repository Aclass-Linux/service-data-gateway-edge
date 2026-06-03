#include "data_gen.h"
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

void data_gen_init(void) {
    static int initialized = 0;
    if (!initialized) {
        srand((unsigned)time(NULL) ^ (unsigned)getpid());
        initialized = 1;
    }
}

double data_gen_get_temperature(void) {
    return 20.0 + (rand() % 801) / 10.0;
}
