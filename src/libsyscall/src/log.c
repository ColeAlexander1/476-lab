
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
#include <stdlib.h>
#include "syscall.h"
#include <stdio.h>
#include <stdarg.h>
#define SC_STAMP_MAX 32

/* The handle of an open log. Callers see only a pointer to it. You define
 * struct log in log.c. It holds at least the file descriptor of the log, and
 * you choose its other fields. */

struct log {
    int fd;
};

/* Open the mission log at path. Returns a handle for log_writef and
 * log_close.
 * Create the file if it does not exist. The lines that the file holds
 * already stay: log_open never removes content. New lines always go to the
 * end of the file, and several processes may write the same log at the same
 * time.
 * Only the owner may read or write the file (mode 0600). This is also true
 * when the file existed before with other permissions. log_open sets this
 * mode with fchmod() after open(), for each file, new or old.
 */
/* Errors:
 * path == NULL gives NULL with EINVAL. open() is retried after EINTR, as in
 * sc_open_write. When the mode cannot be set to 0600, the log is closed,
 * and the result is NULL with the errno of fchmod(), such as EPERM. Any
 * other failure gives NULL with the errno of the call that failed.
 */
log_t *log_open(const char *path){// function call would look like log_t *lg = log_open("something")
   
    if (path == NULL){
        errno = EINVAL; 
        return NULL;
    }
    
    int fd;
    mode_t mode = 0600;
    fd = open(path, O_RDWR | O_APPEND | O_CREAT, mode);
   
    while((fd==-1)&&(errno==EINTR)){

        fd = open(path, O_RDWR | O_APPEND | O_CREAT, mode); 
    } 
    
   
    if (fd == -1) {
        return NULL;
    }
    int status = fchmod(fd, mode);
    if (status == -1) {
        int fchmoderrno = errno;
        sc_close(fd);
        errno = fchmoderrno;
        return NULL;
    }
    log_t *file;
    file = malloc(sizeof(*file));
    if (file == NULL){       
        sc_close(fd);
        return NULL;
    }
    file->fd = fd; //same as (*file).fd = fd;
    return file;

}

/* Write one line to the log, with a message formatted like printf. Returns 0.
 *
 *     <timestamp> <LEVEL> <message>\n
 *     1755534061.482913 INFO     contact ACCF-4587 from NWS
 *
 * The timestamp is the sc_stamp format. The level name is left-aligned in a
 * field of 8 characters, and one space follows the field.
 * A line is at most 512 bytes, including its '\n'. A longer message is cut at
 * its end, and the line still ends with '\n'.
 * A line never mixes with a line from another writer, and no line is lost.
 * To make this true, format the whole line into one buffer first, then write
 * the buffer with sc_write_all on the log, which log_open opened with
 * O_APPEND. On a regular file, Linux writes a buffer this short in one
 * write() call, so no other line can come between two parts of it.
 */
/* Errors:
 * lg == NULL, fmt == NULL or an unknown level gives -1 with EINVAL, and
 * writes nothing. A format that vsnprintf() cannot convert gives -1 with
 * EOVERFLOW, and writes nothing. A failed write gives -1 with the errno of
 * write(). Each '\n' in the message becomes a space, also a '\n' at its end,
 * so one call always writes one line.
 */
int log_writef(log_t *lg, log_level_t level, const char *fmt, ...){
   
   if(lg == NULL || fmt == NULL){
    errno = EINVAL;
    return -1;
   }

    char ts[SC_STAMP_MAX]; 
    int tslen = sc_stamp(ts, sizeof(ts));
    if(tslen == -1){
        return -1;
    }
    char *lvl = log_level_name(level); 
    if(lvl == NULL){
        errno = EINVAL;
        return -1;
    }
    char buf[512]; 
    int bytes = snprintf(buf, sizeof(buf), "%s %-8s ", ts, lvl);

    if (bytes < 0){
        errno = EOVERFLOW; 
        return -1;
    }
    
    size_t max_size = 512 - bytes -2;//remaining size for message, room for newline at the end

    char message[512]; 
    va_list args; 
    va_start(args, fmt);
    int msgbytes = vsnprintf(message, max_size + 1, fmt, args); //max size + 1 because we need room for the null terminator
    va_end(args);


    if(msgbytes < 0){
        errno = EOVERFLOW;
        return -1;
    }

    size_t max = (size_t)msgbytes;

    if(max > max_size){ //if the number of bytes written exceeds the size of the message, update max to max_size
        max = max_size;
    }

    for(int i = 0; i < max; i++){
        if(message[i] == '\n'){
            message[i] = ' '; 
        }
    }

    strcat(buf, message);
    strcat(buf, "\n");
    size_t write_amount = strlen(buf);

    int result = sc_write_all(lg->fd, buf, write_amount);

    if(result == -1){
        return -1;
    }

    return 0;
       
}

/* Close the log and free the handle. Returns 0. */
/* Errors:
 * lg == NULL returns 0. EINTR from close() is not a failure, as in sc_close.
 * Any other failed close gives -1 with the errno of close().
 * The handle is freed in all cases, so do not use it again.
 */
int log_close(log_t *lg){
    if(lg == NULL) {return 0;}
        int result = sc_close(lg -> fd);
            free(lg); 

    return result; 
}

/* The name of a level, as the comment of log_level_t gives it: "SYSTEM",
 * "INFO", "PENDING", "PRIORITY", "ALERT" or "TRACK". */
/* Errors:
 * A value that is not a log_level_t gives NULL. errno does not change.
 */
const char *log_level_name(log_level_t level){

    if (level == LOG_SYSTEM) {return "SYSTEM";}
    if (level == LOG_INFO){return "INFO";}
    if (level == LOG_PENDING){return "PENDING";}
    if (level == LOG_PRIORITY){return "PRIORITY";}
    if (level == LOG_ALERT){return "ALERT";}
    if (level == LOG_TRACK){return "TRACK";}

    return NULL;
}