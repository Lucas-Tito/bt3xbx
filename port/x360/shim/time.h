#ifndef X360_SHIM_TIME_H
#define X360_SHIM_TIME_H
/* time.h da CRT mais o relógio POSIX que o port usa; port/x360/src/posix360.c implementa no 360 */
typedef long time_t;
typedef long clock_t;
struct timespec { long tv_sec; long tv_nsec; };
#define CLOCK_MONOTONIC 1
#define CLOCK_REALTIME 0
int clock_gettime(int clk, struct timespec *ts);
int nanosleep(const struct timespec *req, struct timespec *rem);
time_t time(time_t *);
clock_t clock(void);
#define CLOCKS_PER_SEC 1000
#endif
