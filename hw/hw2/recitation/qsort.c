#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

/* Typedefs */

typedef uint32_t data_t;

// A utility function to swap two elements
void swap(data_t* a, data_t* b) {
  data_t t = *a;
  *a = *b;
  *b = t;
}

/* This function is same in both iterative and recursive*/
int partition(data_t arr[], int l, int h) {
  data_t x = arr[h];
  int i = (l - 1);

  for (int j = l; j <= h - 1; j++) {
    printf("i: %d, j: %d\n", i, j);
    if (arr[j] <= x) {
      printf("arr[j]: %d <= x: %d\n", arr[j], x);
      i++;
      printf("Swapping arr[i] %d and arr[j] %d\n", arr[i], arr[j]);
      swap(&arr[i], &arr[j]);
    }
  }
  printf("Swapping arr[i + 1] %d and arr[j] %d (pivot)\n", arr[i + 1], arr[h]);
  swap(&arr[i + 1], &arr[h]);
  return (i + 1);
}

/* arr[] --> Array to be sorted,
   l  --> Starting index,
   h  --> Ending index */
void quickSortIterative(data_t arr[], int l, int h) {
  // Create an auxiliary stack
  int stack[ h - l + 1 ];

  // initialize top of stack
  int top = -1;

  // push initial values of l and h to stack
  stack[ ++top ] = l;
  stack[ ++top ] = h - 1;

  // Keep popping from stack while is not empty
  while (top >= 0) {
    // Pop h and l
    h = stack[ top-- ];
    l = stack[ top-- ];

    // Set pivot element at its correct position
    // in sorted array
    printf("Calling partition with arr, l: %d, h: %d\n", l, h);
    int p = partition(arr, l, h);

    // If there are elements on left side of pivot,
    // then push left side to stack
    if (p - 1 > l) {
      stack[ ++top ] = l;
      stack[ ++top ] = p - 1;
    }

    // If there are elements on right side of pivot,
    // then push right side to stack
    if (p + 1 < h) {
      stack[ ++top ] = p + 1;
      stack[ ++top ] = h;
    }
  }
}


