
#ifndef _FILE_SYSCALL_H_
#define _FILE_SYSCALL_H_


int sys_open(userptr_t filename,int flag,int *fd); //ignored mode
int sys_close(int fd); 


#endif /* _FILE_SYSCALL_H */