/* rs_decoding.c

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
   invert_matrix() was written by Jim Plank in sd_code.c
 */
int invert_matrix(int *mat, int *inv, int rows, gf_t *gf)
{
	int cols, i, j, k, x, rs2;
	int row_start, tmp, inverse;

	cols = rows;

	k = 0;
	for (i = 0; i < rows; i++) {
		for (j = 0; j < cols; j++) {
			inv[k] = (i == j) ? 1 : 0;
			k++;
		}
	}

	/* First -- convert into upper triangular  */
	for (i = 0; i < cols; i++) {
		row_start = cols*i;

		/* Swap rows if we ave a zero i,i element.  If we can't swap, then the 
		   matrix was not invertible  */

		if (mat[row_start+i] == 0) { 
			for (j = i+1; j < rows && mat[cols*j+i] == 0; j++) ;
			if (j == rows) return -1;
			rs2 = j*cols;
			for (k = 0; k < cols; k++) {
				tmp = mat[row_start+k];
				mat[row_start+k] = mat[rs2+k];
				mat[rs2+k] = tmp;
				tmp = inv[row_start+k];
				inv[row_start+k] = inv[rs2+k];
				inv[rs2+k] = tmp;
			}
		}

		/* Multiply the row by 1/element i,i  */
		tmp = mat[row_start+i];
		if (tmp != 1) {
			inverse = gf->divide.w32(gf, 1, tmp);
			for (j = 0; j < cols; j++) { 
				mat[row_start+j] = gf->multiply.w32(gf, mat[row_start+j], inverse);
				inv[row_start+j] = gf->multiply.w32(gf, inv[row_start+j], inverse);
			}
		}

		/* Now for each j>i, add A_ji*Ai to Aj  */
		k = row_start+i;
		for (j = i+1; j != cols; j++) {
			k += cols;
			if (mat[k] != 0) {
				if (mat[k] == 1) {
					rs2 = cols*j;
					for (x = 0; x < cols; x++) {
						mat[rs2+x] ^= mat[row_start+x];
						inv[rs2+x] ^= inv[row_start+x];
					}
				} else {
					tmp = mat[k];
					rs2 = cols*j;
					for (x = 0; x < cols; x++) {
						mat[rs2+x] ^= gf->multiply.w32(gf, tmp, mat[row_start+x]);
						inv[rs2+x] ^= gf->multiply.w32(gf, tmp, inv[row_start+x]);
					}
				}
			}
		}
	}

	/* Now the matrix is upper triangular.  Start at the top and multiply down  */

	for (i = rows-1; i >= 0; i--) {
		row_start = i*cols;
		for (j = 0; j < i; j++) {
			rs2 = j*cols;
			if (mat[rs2+i] != 0) {
				tmp = mat[rs2+i];
				mat[rs2+i] = 0; 
				for (k = 0; k < cols; k++) {
					inv[rs2+k] ^= gf->multiply.w32(gf, tmp, inv[row_start+k]);
				}
			}
		}
	}
	return 0;
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

void rs_worst_chunk_erasure(int n, int m, int *erased) {
	int i;

	for (i = 0; i < n; i++) erased[i] = 0;
	for (i = 0; i < m; i++) erased[i] = 1;

	return;	
}

void rs_usage(char *s)
{
	fprintf(stderr, "usage: rs_decoding n m r w size \n\n");
	fprintf(stderr, "Output format: n m mult_xor bandwidth(MB/s).\n");
	fprintf(stderr, "Here, the bandwidth is the avg of 10 runs.\n");
	if (s != NULL) fprintf(stderr, "%s\n", s);
	exit(1);
}

int main(int argc, char **argv)
{
	gf_t gfs, gfm;
	int n, m, r, w, size, coef;
	int *coding_matrix, *check_matrix, *decoding_matrix;
	int *parity_layout;
	int *erased, *eindex, *rindex;
	int *encoder, *inverse;
	uint8_t **array;
//	uint8_t **array_copy;
	double timer, split, bandwidth, bandwidth_avg;
	int i, j, k;
	int runs;

	/*parse command parameters*/
	if (argc != 6) rs_usage("Wrong number of arguments");
	if (sscanf(argv[1], "%d", &n) == 0 || n <= 0) rs_usage("Bad n");
	if (sscanf(argv[2], "%d", &m) == 0 || m <= 0 || m >= n) rs_usage("Bad m");
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

	if (!gf_init_easy(&gfs, w)) {
		printf("Bad gf spec\n");
		exit(1);
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
	coding_matrix = cauchy_coding_matrix(n-m, m, w);


	parity_layout = talloc(int, n*r);
	for (i = 0; i < r; i++) {
		for (j = 0; j < n-m; j++) parity_layout[n*i+j] = 0;
		for (j = n-m; j < n; j++) parity_layout[n*i+j] = 1;
	}

	for (i = 0; i < n*r; i++) {
		if (parity_layout[i]) bzero(array[i], size);
	}

	/*encode the parities from the data*/		
	for (i = 0; i < r; i++) {
		for (j = 0; j < m; j++) {
			for (k = 0; k < n-m; k++) {
				coef = coding_matrix[(n-m)*j+k];
				gfm.multiply_region.w32(&gfm, array[n*i+k], array[n*i+(n-m)+j], coef, size, 1);
			}
		}
	}

//	array_copy = talloc(uint8_t *, n*r);
//	for (i = 0; i < n*r; i++) array_copy[i] = talloc(uint8_t, size);
//	for (i = 0; i < n*r; i++) {
//		for (j = 0; j < size; j++) {
//			array_copy[i][j] = array[i][j];
//		}
//	}

	/*test decoding speed...*/	
	
	check_matrix = talloc(int, n*m);
	for (i = 0; i < m; i++) {
		for (j = 0; j < n-m; j++) {
			check_matrix[n*i+j] = coding_matrix[(n-m)*i+j];
		}
		for (j = 0; j < m; j++) {
			if (i == j) {
				check_matrix[n*i+(n-m)+j] = 1;
			}
			else {
				check_matrix[n*i+(n-m)+j] = 0;
			}
		}
	}

	decoding_matrix = talloc(int, (n-m)*m);
	erased = talloc(int, n);
	eindex = talloc(int, m);
	rindex = talloc(int, n-m);	
	encoder = talloc(int, m*m);
	inverse = talloc(int, m*m);

	runs = 0;
	bandwidth_avg = 0;

	while (runs < TEST_TIMES+WARMUP_TIMES) {	
		rs_worst_chunk_erasure(n, m, erased);		
		for (i = 0; i < r; i++) {
			for (j = 0; j < n; j++) {
				if (erased[j]) bzero(array[n*i+j], size);
			}
		}		

		timer_start(&timer);

		i = 0;
		j = 0;
		for (k = 0; k < n; k++) {
			if (erased[k]) {
				eindex[i] = k;
				i++;
			}
			else {
				rindex[j] = k;
				j++;
			}
		}

		for (i = 0; i < m; i++) {
			for (j = 0; j < m; j++) {
				encoder[m*i+j] = check_matrix[n*i+eindex[j]];
			}
		}

		/*generate the inverse of encoder*/
		if (invert_matrix(encoder, inverse, m, &gfs) == -1) {
			printf("Can't invert --- it seems that there is a problem on the coding matrix?\n");
			exit(1);
		}


		for (i = 0; i < m; i++) {
			for (j = 0; j < n-m; j++) {
				decoding_matrix[(n-m)*i+j] = 0;
				for (k = 0; k < m; k++) {
					decoding_matrix[(n-m)*i+j] ^= gfs.multiply.w32(&gfs, inverse[m*i+k], check_matrix[n*k+rindex[j]]);
				}
			}
		}

		for (i = 0; i < m; i++) {
			for (j = 0; j < n-m; j++) {
				coef = decoding_matrix[(n-m)*i+j];
				for (k = 0; k < r; k++) {
					gfm.multiply_region.w32(&gfm, array[n*k+rindex[j]], array[n*k+eindex[i]], coef, size, 1);
				}
			}
		}		

		split = timer_split(&timer);

		//		for (i = 0; i < m; i++) {
		//			for (j = 0; j < r; j++) {
		//				for (k = 0; k < size; k++) {
		//				if (array[n*j+eindex[i]][k] != array_copy[n*j+eindex[i]][k]) {
		//					printf("\nThere is something wrong:(\n\n");
		//					exit(1);
		//				}
		//			}
		//			}
		//		}		

		bandwidth = size*((n-m)*r)/split/1024/1024;
		
		if (runs >= WARMUP_TIMES) bandwidth_avg += bandwidth;

		runs++;
	}

	bandwidth_avg /= TEST_TIMES;

	printf("%2d %2d %2d %2d %.6lf\n", n, m, r, w, bandwidth_avg);

	for (i = 0; i < n*r; i++) free(array[i]);
	free(array);
//	for (i = 0; i < n*r; i++) free(array_copy[i]);
//	free(array_copy);
	
	free(coding_matrix);
	free(parity_layout);
	
	free(check_matrix);
	free(decoding_matrix);
	free(erased);
	free(eindex);
	free(rindex);
	
	free(encoder);
	free(inverse);

	return 0;
}

