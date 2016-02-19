/*
 * Copyright (c) 2001, 2002, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Driver code is in kern/tests/synchprobs.c We will
 * replace that file. This file is yours to modify as you see fit.
 *
 * You should implement your solution to the whalemating problem below.
 */

#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

/*
 * Called by the driver during initialization.
 */

volatile int male_count;
volatile int female_count;
volatile int matchmaker_count;
volatile int mating_count;

struct lock *male_lock;
struct lock *female_lock;
struct lock *matchmaker_lock;
struct lock *mating_lock;

struct cv *male_cv;
struct cv *female_cv;
struct cv *matchmaker_cv;
struct cv *mating_cv;


void whalemating_init() {

	/*
	* Initialize all counts to zero
	*/
	male_count = female_count = matchmaker_count = 0;
	mating_count = 1;
	
	/*
	* Create locks for manipulating counts
	*/
	if (male_lock==NULL) {
		male_lock = lock_create("male_lock");
		if (male_lock == NULL) {
			panic("male_lock: lock_create failed\n");
		}
	}
	if (female_lock==NULL) {
		female_lock = lock_create("female_lock");
		if (female_lock == NULL) {
			panic("female_lock: lock_create failed\n");
		}
	}
	if (matchmaker_lock==NULL) {
		matchmaker_lock = lock_create("matchmaker_lock");
		if (matchmaker_lock == NULL) {
			panic("matchmaker_lock: lock_create failed\n");
		}
	}
	if (mating_lock==NULL) {
		mating_lock = lock_create("mating_lock");
		if (mating_lock == NULL) {
			panic("mating_lock: lock_create failed\n");
		}
	}

	/*
	* Create conditinon variables till we find suitable other halves
	*/
	if (male_cv==NULL) {
		male_cv = cv_create("male_cv");
		if (male_cv == NULL) {
			panic("male_cv: cv_create failed\n");
		}
	}
	if (female_cv==NULL) {
		female_cv = cv_create("female_cv");
		if (female_cv == NULL) {
			panic("female_cv: cv_create failed\n");
		}
	}
	if (matchmaker_cv==NULL) {
		matchmaker_cv = cv_create("matchmaker_cv");
		if (matchmaker_cv == NULL) {
			panic("matchmaker_cv: cv_create failed\n");
		}
	}
	if (mating_cv==NULL) {
		mating_cv = cv_create("mating_cv");
		if (mating_cv == NULL) {
			panic("mating_cv: cv_create failed\n");
		}
	}
	return;
}

/*
 * Called by the driver during teardown.
 */

void
whalemating_cleanup() {
	lock_destroy(male_lock);
	cv_destroy(male_cv);
	lock_destroy(female_lock);
	cv_destroy(female_cv);
	lock_destroy(matchmaker_lock);
	cv_destroy(matchmaker_cv);
	lock_destroy(mating_lock);
	cv_destroy(mating_cv);
	male_lock=NULL;
	male_cv=NULL;
	female_lock=NULL;
	female_cv=NULL;
	matchmaker_lock=NULL;
	matchmaker_cv=NULL;
	mating_lock=NULL;
	mating_cv=NULL;
	male_count = female_count = matchmaker_count = 0;
	mating_count = 1;
	return;
}

void
male(uint32_t index)
{
	(void)index;
	/*
	 * Implement this function by calling male_start and male_end when
	 * appropriate.
	 */
	 // TODO Add kasserts
	male_start(index);
	lock_acquire(male_lock);
	// kprintf_n(" Male %d enter, male_count is %d \n", index,male_count);
	while(male_count > 0){
		// Wait on the male channel
		// kprintf_n(" Male %d waiting on male channel \n", index);
		cv_wait(male_cv, male_lock);
	}
	male_count++;
	lock_acquire(mating_lock);
	if (mating_count < 3){
		// Wait on the mating channel
		// kprintf_n(" Male %d waiting on mating channel as mating count is %d and male_count is %d \n", index, mating_count,male_count);
		mating_count++;
		lock_release(male_lock);
		cv_wait(mating_cv, mating_lock);
		lock_acquire(male_lock);
		mating_count--;
	}
	// Broadcast and start mating
	// kprintf_n(" Mating started by male %d as mating count is %d \n", index, mating_count);
	 cv_broadcast(mating_cv, mating_lock);
	male_count--;

	lock_release(mating_lock);	
	cv_signal(male_cv,male_lock);
	lock_release(male_lock);
	male_end(index); 
	return;
}

void
female(uint32_t index)
{
	(void)index;
	/*
	 * Implement this function by calling female_start and female_end when
	 * appropriate.
	 */
	female_start(index);
	lock_acquire(female_lock);
	// kprintf_n(" Female %d enter, female_count is %d \n", index,female_count);
	while(female_count > 0){
		// Wait on the female channel
	// kprintf_n(" Female %d waiting on female channel \n", index);
		cv_wait(female_cv, female_lock);
	}
	female_count++;
	lock_acquire(mating_lock);
	if (mating_count < 3){
		// Wait on the mating channel
	// kprintf_n(" Female %d waiting on mating channel as mating count is %d and female_count is %d \n", index, mating_count,female_count);
		mating_count++;
		lock_release(female_lock);
		cv_wait(mating_cv, mating_lock);
		lock_acquire(female_lock);
		mating_count--;
	}
	// Broadcast and start mating
	// kprintf_n(" Mating started by female %d as mating count is %d \n", index, mating_count);
	 cv_broadcast(mating_cv, mating_lock);
	female_count--;
	lock_release(mating_lock);	
	cv_signal(female_cv,female_lock);
	lock_release(female_lock);
	female_end(index);
	return;
}

void
matchmaker(uint32_t index)
{
	(void)index;
	/*
	 * Implement this function by calling matchmaker_start and matchmaker_end
	 * when appropriate.
	 */
	 
  	 // kprintf_n(" Maker %d enter and maker count is %d \n", index,matchmaker_count);
	matchmaker_start(index); 
	lock_acquire(matchmaker_lock);
	while(matchmaker_count > 0){
		// Wait on the matchmaker channel
		// kprintf_n(" Maker %d waiting on Maker channel as maker count is %d \n", index,matchmaker_count);
		cv_wait(matchmaker_cv, matchmaker_lock);
	}
	matchmaker_count++;
	lock_acquire(mating_lock);
	if (mating_count < 3){
		// Wait on the mating channel	
	// kprintf_n(" Maker %d waiting on mating channel as mating count is %d and maker count is %d \n", index, mating_count,matchmaker_count);
		mating_count++;
		lock_release(matchmaker_lock);
		cv_wait(mating_cv, mating_lock);
		lock_acquire(matchmaker_lock);
		mating_count--;
	}
	// Broadcast and start mating
	kprintf_n(" Mating started by matchmaker %d \n", index);
	 cv_broadcast(mating_cv, mating_lock);
	matchmaker_count--;
	lock_release(mating_lock);
	cv_signal(matchmaker_cv, matchmaker_lock);
	lock_release(matchmaker_lock);
	matchmaker_end(index);
	return;
}
