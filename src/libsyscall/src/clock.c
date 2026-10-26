
#include <stddef.h>    
#include <sys/types.h> 
#include <errno.h>     
#include <unistd.h>    
#include <fcntl.h>     
#include <sys/stat.h>  
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <string.h>
/* Bytes that a timestamp from sc_stamp needs, including the '\0'. */
#define SC_STAMP_MAX 32

/* Nanoseconds from a clock that never goes backwards. The count starts at an
 * arbitrary point, so the value is not a date. Use it to measure durations.
 */
/* Errors:
 * It cannot fail.
 */
int64_t sc_now_ns(void){
    struct timespec ts;
    int64_t ns;
    clock_gettime(CLOCK_MONOTONIC, &ts); 
    int64_t s_to_ns = ts.tv_sec*1000000000;
    ns = ts.tv_nsec + s_to_ns;
    return ns;
    
}

/* The current date and time: seconds since the epoch, and microseconds from
 * 0 to 999999. A NULL pointer is allowed, and nothing is written to it.
 */
/* Errors:
 * It cannot fail.
 */
void sc_now_wall(int64_t *sec, int32_t *usec)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    if (sec != NULL){
        *sec = ts.tv_sec;
    }if (usec != NULL){
        *usec = ts.tv_nsec / 1000;
    }
}

/* Write the current date and time into buf as "<seconds>.<microseconds>",
 * with exactly 6 digits after the point: "1755534061.000042". buflen is the
 * size of buf, and must hold the stamp and its '\0': SC_STAMP_MAX is enough.
 * Returns the length of the stamp, not counting the '\0'.
 */
/* Errors:
 * buf == NULL gives -1 with EINVAL. A buffer that is too small gives -1 with
 * ERANGE. On a failure, buf is not changed.
 */
int sc_stamp(char *buf, size_t buflen){
    if (buf == NULL){
        errno = EINVAL;
        return -1;
    }
    else if(buflen < SC_STAMP_MAX){
        errno = ERANGE;
        return -1;
    }
    
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    int stamplen = snprintf(buf, buflen, "%lld.%06ld", (long long)ts.tv_sec, ts.tv_nsec/1000);

   
    return stamplen;
}
