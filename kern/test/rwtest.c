/*
 * All the contents of this file are overwritten during automated
 * testing. Please consider this before changing anything in this file.
 */

#define CREATELOOPS     8
#define NTHREADS      32


#include <types.h>
#include <lib.h>
#include <clock.h>
#include <thread.h>
#include <synch.h>
#include <test.h>
#include <kern/secret.h>
#include <spinlock.h>

void 
rwlocktestthread1(void *junk, unsigned long num){

}

void 
rwlocktestthread2(void *junk, unsigned long num){
    
}

int rwtest(int nargs, char **args) {
	(void)nargs;
	(void)args;

    (void)nargs;
    (void)args;

    int i, result;

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
        result = thread_fork("synchtest", NULL, rwlocktestthread1, NULL, i);
        if (result) {
            panic("lt1: thread_fork failed: %s\n", strerror(result));
        }
        result = thread_fork("synchtest", NULL, rwlocktestthread2, NULL, i);
        if (result) {
            panic("lt1: thread_fork failed: %s\n", strerror(result));
        }
    }
    for (i=0; i<NTHREADS; i++) {
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