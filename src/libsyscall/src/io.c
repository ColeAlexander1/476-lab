#include <stddef.h>    
#include <sys/types.h> 
#include <errno.h>     
#include <unistd.h>    
#include <fcntl.h>     
#include <sys/stat.h>  

/* Open path for reading. Returns a file descriptor. */
/* Errors:
 * open() is retried after EINTR. Any other failure gives -1 with the errno
 * of open(), such as ENOENT or EACCES.
 */
int sc_open_read(const char *path){

    int fd;
    fd = open(path, O_RDONLY | O_CLOEXEC);

    while((fd==-1)&&(errno==EINTR)){

        fd = open(path, O_RDONLY | O_CLOEXEC);
    }
    return fd;
}

/* Open path for writing. Returns a file descriptor.
 * Create the file if it does not exist, with the permission bits 0644. The
 * umask of the process masks them, as it does for open(): with umask 027 the
 * file gets 0640. Do not change the mode after the file opens.
 *   append != 0   every write goes to the end of the file, also when other
 *                 processes write the same file at the same time. The content
 *                 that the file holds already stays.
 *   append == 0   the existing content is removed when the file opens.
 */
/* Errors:
 * open() is retried after EINTR. Any other failure gives -1 with the errno
 * of open().
 */
int sc_open_write(const char *path, int append){
    
    int fd;
    if(append != 0){
    fd = open(path, O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC, 0644);
    }
    else {
    fd = open(path, O_WRONLY | O_TRUNC | O_CREAT | O_CLOEXEC, 0644);
    }
  
    while((fd==-1)&&(errno==EINTR)){

        if(append != 0){
        fd = open(path, O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC, 0644);
        }
        else {
        fd = open(path, O_WRONLY | O_TRUNC | O_CREAT | O_CLOEXEC, 0644);
    }

    }  
    return fd;
}

/* The same as sc_open_write, but a new file gets the permission bits in mode,
 * masked by the umask in the same way. Returns a file descriptor. */
/* Errors:
 * open() is retried after EINTR. Any other failure gives -1 with the errno
 * of open().
 */
int sc_open_write_mode(const char *path, int append, mode_t mode){
    
    int fd;
    if(append != 0){
    fd = open(path, O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC, mode);
    }
    else {
    fd = open(path, O_WRONLY | O_TRUNC | O_CREAT | O_CLOEXEC, mode);
    }
  
    while((fd==-1)&&(errno==EINTR)){

        if(append != 0){
        fd = open(path, O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC, mode);
        }
        else {
        fd = open(path, O_WRONLY | O_TRUNC | O_CREAT | O_CLOEXEC, mode);
    }

    }  
    return fd;
}

/* Close fd. Returns 0. */
/* Errors:
 * EINTR is not a failure: Linux has released fd already, so the function
 * returns 0. Any other failure gives -1 with the errno of close(), such as
 * EBADF.
 */
int sc_close(int fd){

    int status = close(fd);

    if ((status == -1) && (errno == EINTR)){
        return 0;
    }
    else if (status == -1){
        return -1;
    }
    return 0;
}

/* Write all n bytes of buf to fd. Returns 0 only when every byte is written.
 * n == 0 writes nothing and returns 0, whatever fd is.
 */
/* Errors:
 * write() is called again after EINTR and after a partial write. write()
 * returning 0 for a non-zero count gives -1 with EIO, because a loop that
 * went on would never end. Any other failure gives -1 with the errno of
 * write(). Bytes written before a failure stay written.
 */
int sc_write_all(int fd, const void *buf, size_t n){

    size_t written = 0;
    ssize_t result; 

    while (written < n){

        result = write(fd, (const char *)buf + written, n - written);

        if (result == -1 && errno == EINTR){
        }
        else if (result == 0){ //returning -1 if result is -1 or 0 (EIO)
            errno = EIO; 
            return -1;
        }
        else if (result == -1){
            return -1; 
        }
        else{
            written = written + result;
        }     

    }
     return 0; 

}

/* Read from fd into buf until n bytes arrive or the input ends. The input
 * ends when read() returns 0.
 * Returns the number of bytes read: n, or fewer only at the end of the input.
 * n == 0 reads nothing and returns 0, whatever fd is.
 */
/* Errors:
 * read() is called again after EINTR. Any other failure gives -1 with the
 * errno of read(), also when some bytes arrived first: they are in buf, but
 * not counted.
 */
ssize_t sc_read_full(int fd, void *buf, size_t n){
    size_t reads = 0;
    ssize_t result; 
    while(reads < n){
        result = read(fd, (char *)buf + reads, n - reads);

        if(result == -1 && errno == EINTR){
            
        }
        else if (result == -1){
            return -1;
        }
        else if (result == 0){
            return reads;
        }
        else {
            reads += result;
        }

    }
   return reads; 
}