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

#include "./matrix_multiply.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>

#include "./tbassert.h"

// Allocates a row-by-cols matrix and returns it
matrix* make_matrix(int rows, int cols) {
  matrix* new_matrix = malloc(sizeof(matrix));  // Leak 32 bytes in 2 objects
  // 4 bytes for rows, 4 bytes for cols, 8 bytes for values = 16 bytes
  // 2 objects of 16 bytes = 32 bytes

  // Set the number of rows and columns
  new_matrix->rows = rows;
  new_matrix->cols = cols;

  // Allocate a buffer big enough to hold the matrix.
  new_matrix->values = (int**)malloc(sizeof(int*) * rows);  // Leak 64 bytes in 2 objects
  // 8 bytes for each pointer, 4 rows = 32 bytes
  // 2 objects of 32 bytes = 64 bytes
  for (int i = 0; i < rows; i++) {
    new_matrix->values[i] = (int*)malloc(sizeof(int) * cols);  // Leak 128 bytes in 8 objects
    // 4 bytes for each int, 4 cols = 16 bytes
    // 8 objects of 16 bytes = 128 bytes
    // 8 objects, 4 iterations = 128 bytes
  }

  return new_matrix;
}

// Frees an allocated matrix
void free_matrix(matrix* m) {
  for (int i = 0; i < m->rows; i++) {
    free(m->values[i]);
  }
  free(m->values);
  free(m);
}

// Print matrix
void print_matrix(const matrix* m) {
  printf("------------\n");
  for (int i = 0; i < m->rows; i++) {
    for (int j = 0; j < m->cols; j++) {
      printf("  %3d  ", m->values[i][j]);
    }
    printf("\n");
  }
  printf("------------\n");
}


// Multiply matrix A*B, store result in C.
int matrix_multiply_run(const matrix* A, const matrix* B, matrix* C) {
  /*
  tbassert(A->cols == B->rows,
           "A->cols = %d, B->rows = %d\n", A->cols, B->rows);
  tbassert(A->rows == C->rows,
           "A->rows = %d, C->rows = %d\n", A->rows, C->rows);
  tbassert(B->cols == C->cols,
           "B->cols = %d, C->cols = %d\n", B->cols, C->cols);
  */

  // n = 1000, 1.56s
  // for (int i = 0; i < A->rows; i++) {
  //  for (int j = 0; j < B->cols; j++) {
  //    for (int k = 0; k < A->cols; k++) {
  //      #ifdef DEBUG
  //      //printf("i: %d, j: %d, k: %d\n", i, j, k);
  //      //printf("A->values[i][k]: %d, B->values[k][j]: %d\n", A->values[i][k], B->values[k][j]);
  //      //printf("Before: C->values[i][j]: %d\n", C->values[i][j]);
  //      #endif
  //      C->values[i][j] += A->values[i][k] * B->values[k][j];
  //      #ifdef DEBUG
  //      //printf("After: C->values[i][j]: %d\n", C->values[i][j]);
  //      #endif
  //    }
  //  }
  // }

  // Change the order of the loops to improve cache performance
  // n = 1000, 0.72s
  for (int i = 0; i < A->rows; i++) {
    for (int k = 0; k < A->cols; k++) {
        for (int j = 0; j < B->cols; j++) {
        C->values[i][j] += A->values[i][k] * B->values[k][j];
      }
    }
  }

  return 0;
}
