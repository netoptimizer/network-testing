/* -*- c-file-style: "linux" -*-
 * Author: Jesper Dangaard Brouer <netoptimizer@brouer.com>
 * License: GPLv2
 *
 * This code is only for testing if the compiler is smart enough to
 * remove the loop statements when the code inside the loop evaluates
 * to nothing.
 */
#define barrier() asm volatile("" : : : "memory")
#define noinline __attribute__((noinline))

#include <stddef.h>
#include <stdio.h>

/* Not using: "static" in-front of "void inline" allow us to view the
 * function in dis-assembly while it still gets inlined in "my_func"
 *
 */
void inline empty_func(void *object)
{
	//barrier(); /* Use barrier to break compiler optimization */
}

void inline empty_for_loop01(int n, void **p)
{
	/* This is clearly empty. Reference for empty func in disasm */
}

void inline empty_for_loop02(int n, void **p)
{
	/* It is important that i and n is same type for the compiler
	 * to remove the loop!
	 */
	int i;

	for (i = 0; i < n; i++) {
		void *object = p[i];
		//barrier();
		empty_func(object);
	}
}

void inline empty_for_loop03(size_t n, void **p)
{
	/* In this case the compilers tested were not smart enough to
	 * remove the loop.
	 *
	 * This only happens if using gcc option: -fno-strict-overflow
	 * ... which the kernel use!
	 *
	 *  Dis-assemble yourself to check your compiler version
	 */
	int i;

	for (i = 0; i < n; i++) {
		void *object = p[i];
		//barrier();
		empty_func(object);
	}
}

void inline empty_for_loop04(unsigned long int n, void **p)
{
	/* See above
	 */
	int i;

	for (i = 0; i < n; i++) {
		void *object = p[i];
		//barrier();
		empty_func(object);
	}
}

void inline empty_for_loop05(size_t n, void **p)
{
	/* Should be empty */
	size_t i;

	for (i = 0; i < n; i++) {
		void *object = p[i];
		//barrier();
		empty_func(object);
	}
}


void noinline my_func(int n)
{
	void *array[42];

	printf("Run: %s\n", __func__);
	empty_for_loop02(n, array);
	empty_for_loop03(n, array);
	empty_for_loop04(n, array);
}

int main(int argc, char *argv[])
{
	printf("Compiler test01\n");
	my_func(1<<31);
	return 0;
}
