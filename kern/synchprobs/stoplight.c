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
 * Driver code is in kern/tests/synchprobs.c We will replace that file. This
 * file is yours to modify as you see fit.
 *
 * You should implement your solution to the stoplight problem below. The
 * quadrant and direction mappings for reference: (although the problem is, of
 * course, stable under rotation)
 *
 *   |0 |
 * -     --
 *    01  1
 * 3  32
 * --    --
 *   | 2|
 *
 * As way to think about it, assuming cars drive on the right: a car entering
 * the intersection from direction X will enter intersection quadrant X first.
 * The semantics of the problem are that once a car enters any quadrant it has
 * to be somewhere in the intersection until it call leaveIntersection(),
 * which it should call while in the final quadrant.
 *
 * As an example, let's say a car approaches the intersection and needs to
 * pass through quadrants 0, 3 and 2. Once you call inQuadrant(0), the car is
 * considered in quadrant 0 until you call inQuadrant(3). After you call
 * inQuadrant(2), the car is considered in quadrant 2 until you call
 * leaveIntersection().
 *
 * You will probably want to write some helper functions to assist with the
 * mappings. Modular arithmetic can help, e.g. a car passing straight through
 * the intersection entering from direction X will leave to direction (X + 2)
 * % 4 and pass through quadrants X and (X + 3) % 4.  Boo-yah.
 *
 * Your solutions below should call the inQuadrant() and leaveIntersection()
 * functions in synchprobs.c to record their progress.
 */

#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

typedef enum Quad{
	ZERO,ONE,TWO,THREE
}Quadrant;

struct lock *lock_zero;
struct lock *lock_one;
struct lock *lock_two;
struct lock *lock_three;

/*
 * Called by the driver during initialization.
 */


void
stoplight_init() {
	/*
	* Create locks for intersection
	*/
	if (lock_zero==NULL) {
		lock_zero = lock_create("lock_zero");
		if (lock_zero == NULL) {
			panic("lock_zero: lock_create failed\n");
		}
	}
	if (lock_one==NULL) {
		lock_one = lock_create("lock_one");
		if (lock_one == NULL) {
			panic("lock_one: lock_create failed\n");
		}
	}
	if (lock_two==NULL) {
		lock_two = lock_create("lock_two");
		if (lock_two == NULL) {
			panic("lock_two: lock_create failed\n");
		}
	}
	if (lock_three==NULL) {
		lock_three = lock_create("lock_three");
		if (lock_three == NULL) {
			panic("lock_three: lock_create failed\n");
		}
	}
	return;
}

/*
 * Called by the driver during teardown.
 */

void stoplight_cleanup() {
	lock_destroy(lock_zero);
	lock_destroy(lock_one);
	lock_destroy(lock_two);
	lock_destroy(lock_three);
	lock_zero = NULL;
	lock_one = NULL;
	lock_two = NULL;
	lock_three = NULL;
	return;
}

void
turnright(uint32_t direction, uint32_t index)
{
	(void)direction;
	(void)index;
	Quadrant _direction = direction;
	switch(_direction){

		case ZERO:
							lock_acquire(lock_zero);
							inQuadrant(ZERO,index);
							leaveIntersection(index);
							lock_release(lock_zero);	
							break;
		case ONE:
							lock_acquire(lock_one);
							inQuadrant(ONE,index);
							leaveIntersection(index);
							lock_release(lock_one);	
							break;
		case TWO:
							lock_acquire(lock_two);
							inQuadrant(TWO,index);
							leaveIntersection(index);
							lock_release(lock_two);	
							break;
		case THREE:
							lock_acquire(lock_three);
							inQuadrant(THREE,index);
							leaveIntersection(index);
							lock_release(lock_three);	
							break;
	}
	
	return;
}
void
gostraight(uint32_t direction, uint32_t index)
{
	(void)direction;
	(void)index;
	/*
	 * Implement this function.
	 */
	 Quadrant _direction = direction;
	 switch(_direction){

		case ZERO:
							lock_acquire(lock_zero);
							inQuadrant(ZERO,index);
							lock_acquire(lock_three);
							lock_release(lock_zero);
							inQuadrant(THREE,index);
							leaveIntersection(index);
							lock_release(lock_three);
							break;
		case ONE:
							lock_acquire(lock_one);
							inQuadrant(ZERO,index);
							lock_acquire(lock_zero);
							lock_release(lock_one);
							inQuadrant(ZERO,index);
							leaveIntersection(index);
							lock_release(lock_zero);	
							break;
		case TWO:
							lock_acquire(lock_two);
							inQuadrant(TWO,index);
							lock_acquire(lock_one);
							lock_release(lock_two);
							inQuadrant(ONE,index);	
							lock_release(lock_one);	
							leaveIntersection(index);
							break;
		case THREE:
							lock_acquire(lock_three);
							inQuadrant(TWO,index);	
							lock_acquire(lock_two);
							lock_release(lock_three);
							inQuadrant(TWO,index);	
							lock_release(lock_two);
							leaveIntersection(index);	
							break;
	}
	return;
}
void
turnleft(uint32_t direction, uint32_t index)
{
	(void)direction;
	(void)index;
	/*
	 * Implement this function.
	 */
	 Quadrant _direction = direction;
		switch(_direction){

		case ZERO:
							lock_acquire(lock_zero);
							inQuadrant(ZERO,index);
							lock_acquire(lock_three);
							lock_release(lock_zero);
							inQuadrant(THREE,index);
							lock_acquire(lock_two);
							lock_release(lock_three);
							inQuadrant(TWO,index);
							lock_release(lock_two);
							leaveIntersection(index);
							break;
		case ONE:
							lock_acquire(lock_one);
							inQuadrant(ONE,index);
							lock_acquire(lock_zero);
							lock_release(lock_one);
							inQuadrant(ZERO,index);
							lock_acquire(lock_three);
							lock_release(lock_zero);
							inQuadrant(THREE,index);
							lock_release(lock_three);	
							leaveIntersection(index);
							break;
		case TWO:
							
							lock_acquire(lock_two);
							inQuadrant(TWO,index);
							lock_acquire(lock_one);
							lock_release(lock_two);
							inQuadrant(ONE,index);
							lock_acquire(lock_zero);
							lock_release(lock_one);
							inQuadrant(ZERO,index);
							lock_release(lock_zero);	
							leaveIntersection(index);
							break;
		case THREE:
							
							lock_acquire(lock_three);
							inQuadrant(THREE,index);
							lock_acquire(lock_two);
							lock_release(lock_three);
							inQuadrant(TWO,index);
							lock_acquire(lock_one);
							lock_release(lock_two);
							inQuadrant(ONE,index);
							lock_release(lock_one);
							leaveIntersection(index);	
							break;
	}
	return;
}


// int get_next_for_straight(curr_quadrant){
	
// 	return -1;	
// };

// int get_next_for_left(curr_quadrant){
// 	return -1;	
// };


