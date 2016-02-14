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
struct lock *lock_turning;

void
_turnRightHelper(struct lock *, Quadrant, uint32_t);

void
_goStraightHelper(struct lock *, struct lock *, Quadrant,Quadrant, uint32_t);

void 
_turnleftHelper(struct lock *,struct lock *,struct lock *,Quadrant ,Quadrant ,Quadrant, uint32_t);

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
	if (lock_turning==NULL) {
		lock_turning = lock_create("lock_three");
		if (lock_turning == NULL) {
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
	lock_destroy(lock_turning);
	lock_zero = NULL;
	lock_one = NULL;
	lock_two = NULL;
	lock_three = NULL;
	lock_turning=NULL;
	return;
}

void
_turnRightHelper(struct lock *first_lock, Quadrant q1, uint32_t index){
	lock_acquire(lock_turning);
	lock_acquire(first_lock);
	inQuadrant(q1,index);
	lock_release(lock_turning);
	leaveIntersection(index);
	lock_release(first_lock);
	return;
}


void
_goStraightHelper(struct lock *first_lock, struct lock *second_lock, Quadrant q1,Quadrant q2, uint32_t index){
	 lock_acquire(lock_turning);
	 lock_acquire(first_lock);
	 inQuadrant(q1,index);
	 lock_acquire(second_lock);
	 inQuadrant(q2,index);
	 lock_release(first_lock);
	 lock_release(lock_turning);	
	 leaveIntersection(index);
	 lock_release(second_lock);
	 return;
}

void 
_turnleftHelper(struct lock *first_lock,struct lock *second_lock,struct lock *third_lock,Quadrant q1,Quadrant q2,Quadrant q3 , uint32_t index){
	lock_acquire(lock_turning);
	lock_acquire(first_lock);
	inQuadrant(q1,index);
	lock_acquire(second_lock);
	inQuadrant(q2,index);
	lock_release(first_lock);
	lock_acquire(third_lock);
	inQuadrant(q3,index);
	lock_release(second_lock);
	lock_release(lock_turning);
	leaveIntersection(index);
	lock_release(third_lock);
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
			_turnRightHelper(lock_zero,ZERO,index);	
			break;
		case ONE:
			_turnRightHelper(lock_one,ONE,index);	
			break;
		case TWO:
			_turnRightHelper(lock_two,TWO,index);	
			break;
		case THREE:
			_turnRightHelper(lock_three,THREE,index);	
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
			_goStraightHelper(lock_zero,lock_three,ZERO,THREE,index);
			break;
		case ONE:
			_goStraightHelper(lock_one,lock_zero,ONE,ZERO,index);
			break;
		case TWO:
			_goStraightHelper(lock_two,lock_one,TWO,ONE,index);
			break;
		case THREE:
			_goStraightHelper(lock_three,lock_two,THREE,TWO,index);
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
			_turnleftHelper(lock_zero,lock_three,lock_two,ZERO,THREE,TWO,index);
			break;
		case ONE:
			_turnleftHelper(lock_one,lock_zero,lock_three,ONE,ZERO,THREE,index);
			break;
		case TWO:
			_turnleftHelper(lock_two,lock_one,lock_zero,TWO,ONE,ZERO,index);
			break;
		case THREE:
			_turnleftHelper(lock_three,lock_two,lock_one,THREE,TWO,ONE,index);
			break;
	}
	return;
}


