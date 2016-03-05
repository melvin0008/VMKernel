/*
 * All the contents of this file are overwritten during automated
 * testing. Please consider this before changing anything in this file.
 */

#define CREATELOOPS 8
#define NTHREADS 32
#define FAIL 1
#define SUCCESS 0

#include <types.h>
#include <lib.h>
#include <clock.h>
#include <thread.h>
#include <synch.h>
#include <test.h>
#include <kern/test161.h>
#include <spinlock.h>

static uint32_t startcount;
static struct cv *startcv;
static struct lock *start_count_lock;
static struct rwlock *testlock = NULL;
static struct semaphore *donesem;
static bool test_status = FAIL;

static
void 
reader_thread(unsigned long num){
    rwlock_acquire_read(testlock);
    kprintf_n("Reader %lu is reading\n",num);
    random_yielder(4);
    kprintf_n("Reader %lu is done reading\n",num);
    rwlock_release_read(testlock);
    V(donesem);
}

static
void 
reader_thread_wrapper(void *junk, unsigned long num){
    (void)junk;   
    random_yielder(4);
    lock_acquire(start_count_lock);
    startcount--;
    if (startcount == 0) {
        cv_broadcast(startcv, start_count_lock);
    } else {
        cv_wait(startcv, start_count_lock);
    }
    lock_release(start_count_lock);
    reader_thread(num);
    V(donesem);
}

static
void 
writer_thread(unsigned long num){
    rwlock_acquire_write(testlock);
    kprintf_n("Writer %lu is writing\n",num);
    random_yielder(4);
    kprintf_n("Write %lu is done\n",num);
    rwlock_release_write(testlock);
    V(donesem);
}

static
void 
writer_thread_wrapper(void *junk, unsigned long num){
    (void)junk;
    random_yielder(4);
    lock_acquire(start_count_lock);
    startcount--;
    if (startcount == 0) {
        cv_broadcast(startcv, start_count_lock);
    } else {
        cv_wait(startcv, start_count_lock);
    }
    lock_release(start_count_lock);
    writer_thread(num);
    V(donesem);
}

/*
 * Use these stubs to test your reader-writer locks.
 */


int rwtest(int nargs, char **args) {
	(void)nargs;
	(void)args;

    int i, result;
    char name[32];
    kprintf_n("Starting rwt1...\n");
    start_count_lock = lock_create("start_count_lock");
    startcv = cv_create("startcv");
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
    // spinlock_init(&status_lock);
    startcount = NTHREADS;
    for (i=0; i<NTHREADS; i++) {
        kprintf_t(".");
        snprintf(name, sizeof(name), "reader- %d", i);
        result = thread_fork(name, NULL, reader_thread_wrapper, NULL, i);
        if (result) {
            panic("rw: thread_fork failed: %s\n", strerror(result));
        }
        snprintf(name, sizeof(name), "writer- %d", i);
        result = thread_fork("synchtest", NULL, writer_thread_wrapper, NULL, i);
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

    // TODO Write a test to check concurrent writes
    test_status = SUCCESS;
    kprintf_t("\n");
	success(test_status, SECRET, "rwt1");
	return 0;
}

int rwtest2(int nargs, char **args) {
	(void)nargs;
	(void)args;

	kprintf_n("rwt2 unimplemented\n");
	success(TEST161_FAIL, SECRET, "rwt2");

	return 0;
}

int rwtest3(int nargs, char **args) {
	(void)nargs;
	(void)args;

	kprintf_n("rwt3 unimplemented\n");
	success(TEST161_FAIL, SECRET, "rwt3");

	return 0;
}

int rwtest4(int nargs, char **args) {
	(void)nargs;
	(void)args;

	kprintf_n("rwt4 unimplemented\n");
	success(TEST161_FAIL, SECRET, "rwt4");

	return 0;
}

int rwtest5(int nargs, char **args) {
	(void)nargs;
	(void)args;

	kprintf_n("rwt5 unimplemented\n");
	success(TEST161_FAIL, SECRET, "rwt5");

	return 0;
}
