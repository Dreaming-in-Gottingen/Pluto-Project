#include "mm_base.h"

int64_t GetSysTimeMonoUs()
{
    int64_t tm;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    tm = ((int64_t)ts.tv_sec*1000000000LL + ts.tv_nsec)/1000LL;
    return tm;
}
