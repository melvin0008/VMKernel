/*
 * All the contents of this file are overwritten during automated
 * testing. Please consider this before changing anything in this file.
 */

#define CREATELOOPS 8
#define NTHREADS 32


#include <types.h>
#include <lib.h>
#include <clock.h>
#include <thread.h>
#include <synch.h>
#include <test.h>
#include <kern/secret.h>
#include <spinlock.h>

static struct lock *testlock = NULL;

void 
reader_thread(void *junk, unsigned long num){
    (void) junk;
    (void) num;
    rwlock_acquire_read(testlock);
    kprintf_n("Reader will read : %l",num);
    random_yielder(4);
    kprintf_n("Reader is done  :%l",num);
    rwlock_release_read(testlock);
    V(donesem);
}

void 
writer_thread(void *junk, unsigned long num){
    (void) junk;
    (void) num;
    rwlock_acquire_write(testlock);
    kprintf_n("Writer is writing  :%l",num);
    random_yielder(4);
    kprintf_n("Write is done  :%l",num);
    rwlock_release_write(testlock);
    V(donesem);

int rwtest(int nargs, char **args) {
	(void)nargs;
	(void)args;

    int i, result;
    char name[32];
    kprintf_n("Starting lt1...\n");
    for (i=0; i<CREATELOOPS; i++) {
        kprintf_t(".");
        testlock = rwlock_create("testlock");
        if (testlock == NULL) {
            panic("lt1: lock_create failed\n");
        }
        donesem = sem_create("donesem", 0);
        if (donesem == NULL) {
            panic("lt1: sem_create failed\n");
        }
        if (i != CREATELOOPS - 1) {
            rwlock_destroy(testlock);
            sem_destroy(donesem);
        }
    }
    spinlock_init(&status_lock);
    test_status = SUCCESS;

    for (i=0; i<NTHREADS; i++) {
        kprintf_t(".");
        snprintf(name, sizeof(name), "reader- %d", i);
        result = thread_fork(name, NULL, reader_thread, NULL, i);
        if (result) {
            panic("rw: thread_fork failed: %s\n", strerror(result));
        }
        snprintf(name, sizeof(name), "writer- %d", i);
        result = thread_fork("synchtest", NULL, writer_thread, NULL, i);
        if (result) {
            panic("rw: thread_fork failed: %s\n", strerror(result));
        }
    }
    for (i=0; i<NTHREADS*2; i++) {
        kprintf_t(".");
        P(donesem);
    }

    rwlock_destroy(testlock);
    sem_destroy(donesem);
    testlock = NULL;
    donesem = NULL;

    kprintf_t("\n");
	success(test_status, SECRET, "rwt1");
	return 0;
}