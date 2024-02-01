// Copyright (c) 2012 MIT License by 6.172 Staff

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

void swap(int* i, int* j) {
  printf("i = %d, j = %d\n", *i, *j);
  printf("i = %p, j = %p\n", i, j);
  int temp = *i;
  *i = *j;
  *j = temp;
  printf("i = %d, j = %d\n", *i, *j);
  printf("i = %p, j = %p\n", i, j);
}

int main() {
  int k = 1;
  int m = 2;
  printf("k = %p, m = %p\n", &k, &m);
  swap(&k, &m);
  printf("k = %p, m = %p\n", &k, &m);
  // What does this print?
  printf("k = %d, m = %d\n", k, m);

  return 0;
}
