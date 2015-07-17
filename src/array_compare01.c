/* Program to measure compare optimizations
 * against several elements in an array
 *
 * Pin to a CPU if TSC is unsable across CPUs
 *  taskset -c 1 ./fast_compare01
 *
 * WARN: This program is a learning experience, I primarily used it to
 * understand how the compiler chooses to optimize and layout the
 * underlying assember code.
 */
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h> /* exit(3) */

#include "global.h"
#include "common.h"

//#define LOOPS 100000000 * 10
#define LOOPS 10000000 * 10

#define N 2

struct entry {
	uint64_t data;
	uint64_t garbage1;
};

static struct entry a[N];
uint64_t match = 104;

#define min(x,y) ((x) < (y)) ? x : y

/* Optimized min without branches */
#define my_min(x, y) ((y) ^ (((x) ^ (y)) & -((x) < (y))))

#define barrier() asm volatile("" : : : "memory")

inline int array_index_match01(uint64_t match)
{
	int i;

	for (i = 0; i < N; i++) {
		if (a[i].data == match) {
			return i;
		}
	}
	return -1;
}


inline int array_index_match02(uint64_t match)
{
	int i;

	for (i = 0; i < N; i++) {
		if ((a[i].data ^ match) == 0)
			return i;
	}
	return -1;
}

inline int array_index_match03(uint64_t match)
{
	int i;
	int res = 0xfffff;
	int ax;

	for (i = 0; i < N; i++) {
		ax = (a[i].data ^ match);
		res = my_min(res, ax);
	}
	if (res >= 0)
		return res;
	return res;
}

/* Start search from a specific index */
inline int array_index_match04(uint64_t match, int index)
{
	int i;
	int res = -1;

	for (i = 0; i < N; i++) {
		if (a[index].data == match) {
			return index;
		}
		if (index++ > N)
			index = 0;
	}
	return res;
}

/* Start search from a specific index */
inline int array_index_match05(uint64_t match, int index)
{
	int i;
	int res = -1;

	for (i = 0; i < N; i++, index++) {
		if (index > N)
			index = 0;
		if (a[index].data == match) {
			return index;
		}
	}
	return res;
}


static int measure01(
	int loops, uint64_t* tsc_begin, uint64_t* tsc_end,
	uint64_t* time_begin, uint64_t* time_end)
{
	int j;
	int res = -1;

	*time_begin = gettime();
	*tsc_begin  = rdtsc();
	for (j = 0; j < loops; j++) {
		res = array_index_match01(match);
		barrier();
	}
	*tsc_end  = rdtsc();
	*time_end = gettime();

	printf("match index[%d]\n", res);

	return j;
}

static int measure02(
	int loops, uint64_t* tsc_begin, uint64_t* tsc_end,
	uint64_t* time_begin, uint64_t* time_end)
{
	int j;
	int res = -1;

	*time_begin = gettime();
	*tsc_begin  = rdtsc();
	for (j = 0; j < loops; j++) {
		res = array_index_match02(match);
		barrier();
	}
	*tsc_end  = rdtsc();
	*time_end = gettime();

	printf("match index[%d]\n", res);

	return j;
}

static int measure03(
	int loops, uint64_t* tsc_begin, uint64_t* tsc_end,
	uint64_t* time_begin, uint64_t* time_end)
{
	int j;
	int res = -1;

	*time_begin = gettime();
	*tsc_begin  = rdtsc();
	for (j = 0; j < loops; j++) {
		res = array_index_match03(match);
		barrier();
	}
	*tsc_end  = rdtsc();
	*time_end = gettime();

	printf("match index[%d]\n", res);

	return j;
}


static int measure04_last_index_search(
	int loops, uint64_t* tsc_begin, uint64_t* tsc_end,
	uint64_t* time_begin, uint64_t* time_end)
{
	int j;
	int res = -1;
	int last_index = 0;

	*time_begin = gettime();
	*tsc_begin  = rdtsc();
	for (j = 0; j < loops; j++) {
		last_index = array_index_match04(match, last_index);
		barrier();
	}
	*tsc_end  = rdtsc();
	*time_end = gettime();

	res = last_index;
	printf("match index[%d]\n", res);

	return j;
}


static int measure05_last_index_search(
	int loops, uint64_t* tsc_begin, uint64_t* tsc_end,
	uint64_t* time_begin, uint64_t* time_end)
{
	int j;
	int res = -1;
	int last_index = 0;

	*time_begin = gettime();
	*tsc_begin  = rdtsc();
	for (j = 0; j < loops; j++) {
		if (!(a[last_index].data == match))
		       	last_index = array_index_match05(match, last_index);
		barrier();
		last_index++;
	}
	*tsc_end  = rdtsc();
	*time_end = gettime();

	res = last_index;
	printf("match index[%d]\n", res);

	return j;
}


static int measure_cmp(
	int loops, uint64_t* tsc_begin, uint64_t* tsc_end,
	uint64_t* time_begin, uint64_t* time_end)
{
	int j;
	int res = -1;

	*time_begin = gettime();
	*tsc_begin  = rdtsc();
	for (j = 0; j < loops; j++) {
		res = (match == j);
		barrier();
	}
	*tsc_end  = rdtsc();
	*time_end = gettime();

	printf("match index[%d]\n", res);

	return j;
}


static int measure0Z(
	int loops, uint64_t* tsc_begin, uint64_t* tsc_end,
	uint64_t* time_begin, uint64_t* time_end)
{
	int j;
	int res = 0;

	int m1 = 1234;
	int m2 = 123456;
	int m3 = match;

	*time_begin = gettime();
	*tsc_begin  = rdtsc();
	for (j = 0; j < loops; j++) {
		if (m1 == j)
			res = j;
		else if (m2 == j)
			res = j;
		else if (m3 == j)
			res = j;
		barrier();
	}
	*tsc_end  = rdtsc();
	*time_end = gettime();

	if (res)
		printf("match index[%d]\n", res);

	return j;
}



int main()
{
	int i;

	printf("Array size: %d\n", N);

	for (i = 0; i < N; i++)
		a[i].data = 1000 + i;

	a[N-1].data = match;
//	a[0].data = match;

	printf("Measuring 0A\n");
	time_func(LOOPS, measure01);

	printf("Measuring 0B\n");
	time_func(LOOPS, measure02);

	printf("Measuring 0C\n");
	time_func(LOOPS, measure03);

	printf("Measuring 0D_last_index_search\n");
	time_func(LOOPS, measure04_last_index_search);

	printf("Measuring 0E_last_index_search\n");
	time_func(LOOPS, measure05_last_index_search);

	printf("Measuring CMP\n");
	time_func(LOOPS, measure_cmp);

	printf("Measuring 0Z\n");
	time_func(LOOPS, measure0Z);

	return 0;
}
