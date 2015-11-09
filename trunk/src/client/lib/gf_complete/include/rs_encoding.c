/* rs_encoding.c

Mingqiang Li
mingqiangli.cn@gmail.com
http://hk.linkedin.com/in/mingqiangli/

Department of Computer Science and Engineering
The Chinese University of Hong Kong
Address: Room 120, 1/F, Ho Sin Hang Engineering Building, The Chinese University of Hong Kong, Shatin, New Territories, Hong Kong

May, 2013
 */

#include <strings.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "galois.h"
#include "jerasure.h"
#include "cauchy.h"
#include "gf_complete.h"

#define talloc(type, num) (type *) malloc(sizeof(type)*(num))

#define GOOD_CAUCHY 1
#define MAX_ERASURE 0

#define WARMUP_TIMES 3
#define TEST_TIMES 10

/*
   timer_start() was written by Jim Plank in sd_code.c
 */
void timer_start(double *t)
{
	struct timeval  tv;

	gettimeofday (&tv, NULL);
	*t = (double)tv.tv_sec + (double)tv.tv_usec * 1e-6;
}

/*
   timer_split() was written by Jim Plank in sd_code.c
 */
double timer_split(const double *t)
{
	struct timeval  tv;
	double  cur_t;

	gettimeofday (&tv, NULL);
	cur_t = (double)tv.tv_sec + (double)tv.tv_usec * 1e-6;
	return (cur_t - *t);
}

int *cauchy_coding_matrix(int k, int m, int w)//k=n-m
{
	int *matrix;

#if GOOD_CAUCHY
	matrix = cauchy_good_general_coding_matrix(k, m, w);
#else
	matrix = cauchy_original_coding_matrix(k, m, w); 
#endif

	if (matrix == NULL) {
		printf("Couldn't make coding matrix --- Please check coding parameters!\n");
		exit(1);
	}

	return matrix;
}

/*
   print_data() was written by Jim Plank in sd_code.c
 */
void print_data(int n, int r, int size, uint8_t **array)
{
	int i, j;

	for (i = 0; i < n*r; i++) {
		for (j = 0; j < size; j++) {
			printf(" %02x", array[i][j]);
		}
		printf("\n");
	}
	printf("\n");
}

void rs_usage(char *s)
{
	fprintf(stderr, "usage: rs_encoding n m r w size \n\n");
	fprintf(stderr, "Output format: n m mult_xor bandwidth(MB/s).\n");
	fprintf(stderr, "Here, the bandwidth is the avg of 10 runs.\n");
	if (s != NULL) fprintf(stderr, "%s\n", s);
	exit(1);
}

int main(int argc, char **argv)
{
	gf_t  gfm;
	int n, m, r, w, size, coef;
	int *matrix_row;
	int *parity_layout;
	uint8_t **array;
	int cbyte;
	double timer, split, bandwidth, bandwidth_avg;
	int i, j, k;
	int mult_xor;
	int runs;

	/*parse command parameters*/
	if (argc != 6) rs_usage("Wrong number of arguments");
	if (sscanf(argv[1], "%d", &n) == 0 || n <= 0) rs_usage("Bad n");
	if (sscanf(argv[2], "%d", &m) == 0 || m < 0 || m >= n) rs_usage("Bad m");
	if (sscanf(argv[3], "%d", &r) == 0 || r <= 0) rs_usage("Bad r");
	if (sscanf(argv[4], "%d", &w) == 0 || w <= 0) rs_usage("Bad w");
	if (sscanf(argv[5], "%d", &size) == 0 || size <= 0) rs_usage("Bad size");
	if (size % 8 != 0) rs_usage("Size has to be a multiple of 8\n");

	if (w == 16 || w == 32) {
		if (!gf_init_hard(&gfm, w, GF_MULT_SPLIT_TABLE, GF_REGION_ALTMAP | GF_REGION_SSE, GF_DIVIDE_DEFAULT, 0, 4, w, NULL, NULL)) {
			printf("Bad gf spec\n");
			exit(1);
		}
	} else if (w == 8) {
		if (!gf_init_easy(&gfm, w)) {
			printf("Bad gf spec\n");
			exit(1);
		}
	} else {
		printf("Not supporting w = %d\n", w);
	}

	array = talloc(uint8_t *, n*r);
	for (i = 0; i < n*r; i++) array[i] = talloc(uint8_t, size);
	srand48(time(0));
	for (i = 0; i < n*r; i++) {
		for (j = 0; j < size; j++) {
			array[i][j] = lrand48()%256;
		}
	}
	//	print_data(n, r, size, array);

	/*create the cauchy coding matrix using Jerasure*/
	if (m > 0) matrix_row = cauchy_coding_matrix(n-m, m, w);

	/*test encoding performance here...*/
	/*
	   printf("\nTest encoding performance...\n\n");
	 */

	parity_layout = talloc(int, n*r);
	for (i = 0; i < r; i++) {
		for (j = 0; j < n-m; j++) parity_layout[n*i+j] = 0;
		for (j = n-m; j < n; j++) parity_layout[n*i+j] = 1;
	}

	runs = 0;
	bandwidth_avg = 0;

	while (runs < TEST_TIMES+WARMUP_TIMES) {		
		for (i = 0; i < n*r; i++) {
			if (parity_layout[i]) bzero(array[i], size);
		}

		timer_start(&timer);

		mult_xor = 0;
		/*encode each of the first r-m_idr rows using the erasure code*/
		
		for (i = 0; i < r; i++) {
			for (j = 0; j < m; j++) {
				for (k = 0; k < n-m; k++) {
					coef = matrix_row[(n-m)*j+k];
					gfm.multiply_region.w32(&gfm, array[n*i+k], array[n*i+(n-m)+j], coef, size, 1);
					mult_xor++;
				}
			}
		}

		split = timer_split(&timer);

		//print_data(n, r, size, array);

		bandwidth = size*((n-m)*r)/split/1024/1024;
		
		if (runs >= WARMUP_TIMES) bandwidth_avg += bandwidth;

		runs++;
	}

	bandwidth_avg /= TEST_TIMES;

	printf("%2d %2d %2d %2d %3d %.6lf\n", n, m, r, w, mult_xor, bandwidth_avg);

	if (m > 0) free(matrix_row);
	free(parity_layout);

	for (i = 0; i < n*r; i++) free(array[i]);
	free(array);

	return 0;
}

