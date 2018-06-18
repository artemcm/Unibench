/**
 * gramschmidt.c: This file was adapted from PolyBench/GPU 1.0 test
 * suite to run on GPU with OpenMP 4.0 pragmas and OpenCL driver.
 *
 * http://www.cse.ohio-state.edu/~pouchet/software/polybench/GPU
 *
 * Contacts: Marcio M Pereira <mpereira@ic.unicamp.br>
 *           Rafael Cardoso F Sousa <rafael.cardoso@students.ic.unicamp.br>
 *           Luís Felipe Mattos <ra107822@students.ic.unicamp.br>
*/

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#ifdef _OPENMP
#include <omp.h>
#endif

#include "BenchmarksUtil.h"

// define the error threshold for the results "not matching"
#define PERCENT_DIFF_ERROR_THRESHOLD 0.05

/* Problem size. */
#ifdef RUN_TEST
#define SIZE 1100
#elif RUN_BENCHMARK
#define SIZE 9600
#else
#define SIZE 1000
#endif

/* Problem size */
#define M SIZE
#define N SIZE

/* Can switch DATA_TYPE between float and double */
typedef float DATA_TYPE;

void gramschmidt(DATA_TYPE *A, DATA_TYPE *R, DATA_TYPE *Q) {
  int i, j, k;
  DATA_TYPE nrm;
  for (k = 0; k < N; k++) {
    nrm = 0;
    for (i = 0; i < M; i++) {
      nrm += A[i * N + k] * A[i * N + k];
    }

    R[k * N + k] = sqrt(nrm);
    for (i = 0; i < M; i++) {
      Q[i * N + k] = A[i * N + k] / R[k * N + k];
    }

    for (j = k + 1; j < N; j++) {
      R[k * N + j] = 0;
      for (i = 0; i < M; i++) {
        R[k * N + j] += Q[i * N + k] * A[i * N + j];
      }
      for (i = 0; i < M; i++) {
        A[i * N + j] = A[i * N + j] - Q[i * N + k] * R[k * N + j];
      }
    }
  }
}

void gramschmidt_OMP(DATA_TYPE *A, DATA_TYPE *R, DATA_TYPE *Q) {
  int i, j, k;
  DATA_TYPE nrm;

  #pragma omp target data map(to: R[:M*N], Q[:M*N]) map(tofrom: A[:M*N]) device(DEVICE_ID)
  {
    for (k = 0; k < N; k++) {
      // CPU
      nrm = 0;
      #pragma omp target update from(A[:M*N])
      for (i = 0; i < M; i++) {
        nrm += A[i * N + k] * A[i * N + k];
      }
      R[k * N + k] = sqrt(nrm);

      for (i = 0; i < M; i++) {
        Q[i * N + k] = A[i * N + k] / R[k * N + k];
      }
      #pragma omp update to(Q[:M*N])
      #pragma omp target teams distribute parallel for private(i)
      for (j = k + 1; j < N; j++) {
        R[k * N + j] = 0;
        for (i = 0; i < M; i++) {
          R[k * N + j] += Q[i * N + k] * A[i * N + j];
        }
        for (i = 0; i < M; i++) {
          A[i * N + j] = A[i * N + j] - Q[i * N + k] * R[k * N + j];
        }
      }
    }
  }
}

void init_array(DATA_TYPE *A, DATA_TYPE *A2) {
  int i, j;

  for (i = 0; i < M; i++) {
    for (j = 0; j < N; j++) {
      A[i * N + j] = ((DATA_TYPE)(i + 1) * (j + 1)) / (M + 1);
      A2[i * N + j] = A[i * N + j];
    }
  }
}

int compareResults(DATA_TYPE *A, DATA_TYPE *A_outputFromGpu) {
  int i, j, fail;
  fail = 0;

  for (i = 0; i < M; i++) {
    for (j = 0; j < N; j++) {
      if (percentDiff(A[i * N + j], A_outputFromGpu[i * N + j]) >
          PERCENT_DIFF_ERROR_THRESHOLD) {
        fail++;
        // printf("i: %d j: %d \n1: %f\n 2: %f\n", i, j, A[i*N + j],
        // A_outputFromGpu[i*N + j]);
      }
    }
  }

  // Print results
  printf("Non-Matching CPU-GPU Outputs Beyond Error Threshold of %4.2f "
         "Percent: %d\n",
         PERCENT_DIFF_ERROR_THRESHOLD, fail);

  return fail;
}

int main(int argc, char *argv[]) {
  double t_start, t_end;
  int fail = 0;

  DATA_TYPE *A;
  DATA_TYPE *A_outputFromGpu;
  DATA_TYPE *R;
  DATA_TYPE *Q;

  A = (DATA_TYPE *)malloc(M * N * sizeof(DATA_TYPE));
  A_outputFromGpu = (DATA_TYPE *)malloc(M * N * sizeof(DATA_TYPE));
  R = (DATA_TYPE *)malloc(M * N * sizeof(DATA_TYPE));
  Q = (DATA_TYPE *)malloc(M * N * sizeof(DATA_TYPE));

  fprintf(stdout, "<< Gram-Schmidt decomposition >>\n");

  init_array(A, A_outputFromGpu);

  t_start = rtclock();
  gramschmidt_OMP(A_outputFromGpu, R, Q);
  t_end = rtclock();
  fprintf(stdout, "GPU Runtime: %0.6lfs\n", t_end - t_start);

#ifdef RUN_TEST
  t_start = rtclock();
  gramschmidt(A, R, Q);
  t_end = rtclock();
  fprintf(stdout, "CPU Runtime: %0.6lfs\n", t_end - t_start);

  fail = compareResults(A, A_outputFromGpu);
#endif

  free(A);
  free(A_outputFromGpu);
  free(R);
  free(Q);

  return fail;
}
