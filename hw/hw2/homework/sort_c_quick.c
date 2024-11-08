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


#include "./util.h"

const int THRESHOLD = 1;

// Function prototypes
extern void isort(data_t* begin, data_t* end);
static int partition(data_t* A, int l, int h);

void sort_c(data_t* A, int l, int h) {
  assert(A);
  assert(l <= h);

  //if ((h-l+1) > THRESHOLD) {
    int p = partition(A,l,h);

    // Sort left if not empty
    if ((p - 1) > l) {
      sort_c(A,l,p-1);
    }

    // Sort right if not empty
    if ((p + 1) < h) { 
      sort_c(A,p+1,h);
    }
  // } else {
  //   isort(A,A+h);
  //}
}

static inline int partition(data_t* A, int l, int h) {
  data_t pivot = *(A+h);
  int i = l, r = l;
  while (i < h) {
    if (*(A+i) < pivot) {
      data_t temp = *(A+i); 
      *(A+i) = *(A+r); 
      *(A+r) = temp; 
      r++;
    }
    i++;
  }
  *(A+h) = *(A+r);
  *(A+r) = pivot;

  return r;
}
