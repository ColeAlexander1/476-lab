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
#include <dirent.h>
/* What the kernel knows about a running process, read from /proc.
 *
 * The directory /proc/<pid> describes the process with that pid. For the
 * calling process, use /proc/self. Most fields come from the text file
 * /proc/<pid>/status, which has one field on each line:
 *
 *     <Key>:<spaces or tabs><value>
 *     VmRSS:	    6736 kB
 *
 * A memory value ends with " kB". Find a field by its whole key, up to and
 * including the ':'. A search for "voluntary_ctxt_switches" at any point in
 * a line also finds "nonvoluntary_ctxt_switches", and gives the wrong number.
 *
 *   field        where the value comes from
 *   pid          the pid argument, or getpid() when the argument is 0
 *   ppid         status, key "PPid"
 *   name         status, key "Name"
 *   vm_rss_kb    status, key "VmRSS", the number without " kB"
 *   vm_size_kb   status, key "VmSize", the number without " kB"
 *   threads      status, key "Threads"
 *   open_fds     the number of entries in the directory /proc/<pid>/fd,
 *                not counting "." and ".."
 *   vol_ctxt     status, key "voluntary_ctxt_switches"
 *   invol_ctxt   status, key "nonvoluntary_ctxt_switches"
 */
typedef struct {
    pid_t pid;
    pid_t ppid;
    char  name[64];      /* the executable name, with no '\n' */
    long  vm_rss_kb;     /* resident memory, in kilobytes     */
    long  vm_size_kb;    /* total virtual memory, in kilobytes */
    int   threads;       /* the number of threads             */
    int   open_fds;      /* the number of open descriptors    */

    /* How often this process stopped running, and who decided.
     *
     * vol_ctxt   it gave the processor up itself, because it had to wait for
     *            something: a read, a lock, a sleep.
     * invol_ctxt the operating system TOOK the processor away, because
     *            another task was owed a turn.
     *
     * A rising involuntary count is the operating system sharing one
     * processor between several tasks. */
    long  vol_ctxt;
    long  invol_ctxt;
} proc_info_t;

/* Fill out for process pid. pid 0 means the calling process. Returns 0.
 * The comment of proc_info_t gives the source of each field.
 * For the calling process (pid 0, or its own pid), open_fds does not count
 * the descriptor that proc_report itself opens to read /proc/<pid>/fd.
 */
/* Errors:
 * out == NULL gives EINVAL. A pid with no process gives ENOENT. A process of
 * another user gives EACCES, because its fd directory cannot be read. A
 * field that /proc does not list, such as VmSize for a kernel thread, is 0.
 * On a failure, the content of out is not defined.
 */
int proc_report(pid_t pid, proc_info_t *out){

    if (out == NULL) {
        errno = EIVAL;
        return -1
    }

    memset(out, 0, sizeof(*out));

    char path[50];

    if(pid == 0){
        snprintf(path, sizeof(path), "/proc/self/status");
        out->pid = getpid();
    } else {
        snprintf(path, sizeof(path), "/proc/%d/status", pid);
        out->pid = pid;
    }

    int fd = sc_open_read(path);
    if (fd == -1){
        return -1;
    }

    char buf[4096];
    ssize_t bytes = sc_read_full(fd, buf, sizeof(buf) - 1);

    if (bytes == -1){
        int saved_errno = errno;
        sc_close(fd);
        errno = saved_errno;
        return -1;
    }

    sc_close(fd);
    buf[bytes] = '\0';

    char *line = buf;

    while (line != NULL){
        char *next = strchr(line, '\n');

        if(next != NULL){
            *next = '\0';
            next++;        
        }

        if (strncmp(line, "Name:", 5) == 0) {
            char *name = line + 5;

            while (*name == ' ' || *name == '\t') {
                name++;
            }
            snprintf(out->name, sizeof(out->name), "%s", name);
        }

        else if (strncmp(line, "PPid:", 5) == 0) {
            out->ppid = (pid_t)strtol(line + 5, NULL, 10);
        } 

        else if (strncmp(line, "Threads:", 8) == 0) {
            out->threads = (int)strtol(line + 8, NULL, 10);
        } 

        else if (strncmp(line, "VmRSS:", 6) == 0) {
            out->vm_rss_kb = strtol(line + 6, NULL, 10);
        } 

        else if (strncmp(line, "VmSize:", 7) == 0) {
            out->vm_size_kb = strtol(line + 7, NULL, 10);
        } 

        else if (strncmp(line, "voluntary_ctxt_switches:", 24) == 0) {
            out->vol_ctxt = strtol(line + 24, NULL, 10);
        } 

        else if (strncmp(line, "nonvoluntary_ctxt_switches:", 27) == 0) {
            out->invol_ctxt = strtol(line + 27, NULL, 10);
        }

        line = next;
    }

    if (pid == 0) {
        snprintf(path, sizeof(path), "/proc/self/fd");
    } 
    
    else {
        snprintf(path, sizeof(path), "/proc/%d/fd", pid);
    }

    DIR = *dir = opendir(path);

    if(dir == NULL){
        return -1;
    }

    struct dirent *entry;

    while(1){
        entry = readdir(dir);
        errno = 0;
        if (entry == NULL){
            if(errno == -1 ){
                int saved errno = errno;
                closedir(dir);
                errno = saved_errno;
                return -1;
            }
            break;
        }
        if(strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0){
            out->open_fds++;
        }
    }
    if (closedir(dir) == -1){
        return -1;
    }

    if (pid == 0){
        out->open_fds--;
    }
    return 0;
}

/* Write info to fd as text, in exactly this layout (%d, %s and %ld as in
 * printf). Returns 0 when the whole report is written.
 *
 *     process report
 *       pid        %d
 *       ppid       %d
 *       name       %s
 *       vm_size    %ld kB
 *       vm_rss     %ld kB
 *       threads    %d
 *       open_fds   %d
 *       ctxt_vol   %ld
 *       ctxt_invol %ld
 *
 * Each line after the first has two spaces, then the label (such as "pid")
 * left-aligned in a field of 10 characters, then one space, then the value.
 * Every line ends with '\n'. The order of the lines is not the order of the
 * fields in proc_info_t: vm_size comes before vm_rss.
 */
/* Errors:
 * info == NULL gives -1 with EINVAL. A failed write gives -1 with the errno
 * of write().
 */
int proc_report_write(int fd, const proc_info_t *info){
    if (info == NULL) {
        errno = EINVAL;
        return -1;
    }

    char buf[512];

   int len = snprintf(buf, sizeof(buf),
    "process report\n"
    "  pid        %d\n"
    "  ppid       %d\n"
    "  name       %s\n"
    "  vm_size    %ld kB\n"
    "  vm_rss     %ld kB\n"
    "  threads    %d\n"
    "  open_fds   %d\n"
    "  ctxt_vol   %ld\n"
    "  ctxt_invol %ld\n",info->pid, info->ppid, info->name, info->vm_size, info->vm_rss, info->threads, info->open_fds, info->ctxt_vol, info->ctxt_invol);

    if (len < 0 ) {
        errno = ERANGE;
        return -1;
    }

    if (sc_write_all(fd, buf, (size_t)len) < 0) {
        return -1;
    }


    return 0;
}



