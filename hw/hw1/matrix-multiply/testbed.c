/**
 * Copyright (c) 2012 MIT License by 6.172 Staff
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 **/


/**
 * testbed.c:
 *
 * This file runs your code, timing its execution and printing out the result.
 **/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "./fasttime.h"
#include "./matrix_multiply.h"


int main(int argc, char** argv) {
  int optchar = 0;
  int show_usec = 0;
  int should_print = 0;
  int use_zero_matrix = 0;

  // Always use the same seed, so that our tests are repeatable.
  unsigned int randomSeed = 1;

  matrix* A;
  matrix* B;
  matrix* C;

  // const int kMatrixSize = 1;
  // const int kMatrixSize = 4;
  const int kMatrixSize = 1000;
  // const int kMatrixSize = 4096;


  // Parse command line arguments
  while ((optchar = getopt(argc, argv, "upz")) != -1) {
    switch (optchar) {
      case 'u':
        show_usec = 1;
        break;
      case 'p':
        should_print = 1;
        break;
      case 'z':
        use_zero_matrix = 1;
        break;
      default:
        printf("Ignoring unrecognized option: %c\n", optchar);
        continue;
    }
  }

  // This is a trick to make the memory bug leads to a wrong output.
  int size = sizeof(int) * 4;  // 4 bytes * 4 = 16 bytes, 4 integers
  int* temp[20];  // 20 pointers to integers

  for (int i = 0; i < 20; i++) {
    // Allocating 16 bytes for each pointer
    // Assigning the pointer to the array of type "int"
    temp[i] = (int*)malloc(size);
    // Filling the first 16 bytes of the allocated memory with 1 byte values a.k.a. 16 "x01".
    // Because the array is divided into type "int"
    // This equals to 4 int's with a value of hexadecimal representation of "0x01010101"
    memset(temp[i], 1, size);
  }
  int total = 0;
  for (int i = 0; i < 20; i++) {
    for (int j = 0; j < 4; j++) {
      // Accessing the jth int (4 bytes)
      // Should be 4 bytes assigned to a value of 0x01010101
      // Adding it to the total
      total += temp[i][j];
    }
  }

  // 0x01010101 = 16843009
  // total should be 20*4*0x01010101 = 20*4*16843009 = 1347440720
  // But, if a memory bug occurs, the total will be different
  printf("Expected: %d\n", 1347440720);
  printf("Actual: %d\n", total);

  // If total is used, then the compiler will not optimize out the mallocs
  if (!total) printf("Trick to stop mallocs from being optimized out.");
  for (int i = 0; i < 20; i++) {
    free(temp[i]);  // Manually free
  }

  fprintf(stderr, "Setup\n");

  // A = make_matrix(kMatrixSize, kMatrixSize+1);
  A = make_matrix(kMatrixSize, kMatrixSize);
  B = make_matrix(kMatrixSize, kMatrixSize);
  C = make_matrix(kMatrixSize, kMatrixSize);

  if (use_zero_matrix) {
    for (int i = 0; i < A->rows; i++) {
      for (int j = 0; j < A->cols; j++) {
        A->values[i][j] = 0;
      }
    }
    for (int i = 0; i < B->rows; i++) {
      for (int j = 0; j < B->cols; j++) {
        B->values[i][j] = 0;
      }
    }
  } else {
    for (int i = 0; i < A->rows; i++) {
      for (int j = 0; j < A->cols; j++) {
        A->values[i][j] = rand_r(&randomSeed) % 10;
      }
    }
    for (int i = 0; i < B->rows; i++) {
      for (int j = 0; j < B->cols; j++) {
        B->values[i][j] = rand_r(&randomSeed) % 10;
      }
    }
  }

  // Zero out C
  for (int i = 0; i < C->rows; i++) {
    for (int j = 0; j < C->cols; j++) {
      C->values[i][j] = 0;
    }
  }

  if (should_print) {
    printf("Matrix A: \n");
    print_matrix(A);

    printf("Matrix B: \n");
    print_matrix(B);
  }

  fprintf(stderr, "Running matrix_multiply_run()...\n");

  fasttime_t time1 = gettime();
  matrix_multiply_run(A, B, C);
  fasttime_t time2 = gettime();

  if (should_print) {
    printf("---- RESULTS ----\n");
    printf("Result: \n");
    print_matrix(C);
    printf("---- END RESULTS ----\n");
  }

  if (show_usec) {
    double elapsed = tdiff(time1, time2);
    printf("Elapsed execution time: %f usec\n",
           elapsed * (1000.0 * 1000.0));
  } else {
    double elapsed = tdiff(time1, time2);
    printf("Elapsed execution time: %f sec\n", elapsed);
  }

  free_matrix(A);
  free_matrix(B);
  free_matrix(C);

  return 0;
}
