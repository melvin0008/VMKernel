
#ifndef _FILE_SYSCALL_H_
#define _FILE_SYSCALL_H_

/*
* 
* The system call to OPEN a file
* Takes a filename, specifies permissions and provides a file descriptor as arguments
* 
*/
int sys_open(userptr_t filename,int flag,int *fd); //ignored mode
/*
* 
* The system call to CLOSE a file
* Takes file descriptor as argument
* 
*/
int sys_close(int fd); 
/*
* 
* The system call to READ from a file
* Takes file descriptor, a buffer, number of files to read and return value address as arguments
* 
*/
int sys_read(int fd, void *buf, size_t buflen, ssize_t *retval);
/*
* 
* The system call to WRITE into a file
* Takes file descriptor, a buffer, number of files to write and return value address as arguments
* 
*/
int sys_write(int fd, void *buf, size_t buflen, ssize_t *retval);

int sys_dup2(int oldfd, int newfd);

int sys_chdir(const char *pathname);

int sys__getcwd(char *buf, size_t buflen, size_t *retval);

#endif /* _FILE_SYSCALL_H */