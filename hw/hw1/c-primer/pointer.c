// Copyright (c) 2012 MIT License by 6.172 Staff

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int main(int argc, char * argv[]) {  // What is the type of argv?

  // argv is a pointer array to each argument passed to the program

  // Pointer arrays allow for multiple character arrays
  // Using a pointer array allows for multi-dimensional array elements, like multiple character arrays
  char *pointerArray[]={"one","two","three"};
  printf("%s\n%s\n%s\n",pointerArray[0],pointerArray[1],pointerArray[2]);

  // First value is name of program
  printf("Program name: %s\n", *argv);
  printf("Program name: %s\n", argv[0]);

  // argc is the length of arguments
  // arguments begin after the program name
  if (argc > 1) {
    printf("First argument: %s\n", argv[1]);
  }

  // Referencing and dereferencing
  int i = 5;
  // The & operator here gets the address of i and stores it into pi
  int * pi = &i;
  printf("int * pi = %p\n", pi);

  // The * operator here dereferences pi and stores the value -- 5 --
  // into j.
  int j = *pi;
  printf("int j = %i\n", j);

  // Arrays and pointers
  char c[] = "6.172"; // A (single) character array
  printf("c[0] = %c\n", c[0]);  

  char * pc = c;  // Valid assignment: c acts like a pointer to c[0] here. i.e. &c[0]
  char d = *pc; // Dereference the pointer, this is equal to the value of c[0]
  printf("char d = %c\n", d);  // What does this print?

  // Name of array is a pointer to the first element in an array
  printf("Array c = %p\n", c); // type of c is char *
  printf("Memory address to first element of c = %p\n", &c[0]);

  // Access array elements through pointers with pointer arithmetic
  printf("c[0] = %c\n", *pc);
  printf("c[1] = %c\n", *(c + 1));
  printf("c[1] = %c\n", *(pc + 1));
  printf("c[2] = %c\n", *(pc + 2));
  printf("c[3] = %c\n", *(pc + 3));
  printf("c[4] = %c\n", *(pc + 4));

  // Use loop to access
  for (i = 0; i < 5; i++) {
    printf("c[%i] = %c\n", i, *(pc + i));
  }

  // Pointer to an entire array
  // It is 1 pointer to 1 array. Different than an array of pointers (pointer array)
  char (*parray)[];
  parray = &c;
  printf("parray = %s\n", *parray); // Now we can access to entire character array, without having to access each element individually

  // compound types are read right to left in C.
  // pcp is a pointer to a pointer to a char, meaning that
  // pcp stores the address of a char pointer.
  char ** pcp;
  pcp = argv;  // Why is this assignment valid? argv acts as pointer to argv[0], pcp is a pointer to this pointer
  
  // argv is a pointer array
  printf("char ** pcp = %p\n", pcp);
  printf("*pcp = %s\n", *pcp); // Dereferencing pcp, means to dereference argv, which is a pointer to the first element.
  printf("pcp[0] = %s\n", pcp[0]); // When you index pcp, you are indexing the array of argv
  if (argc > 1) {
    printf("pcp[1] = %s\n", pcp[1]); // When you index pcp, you are indexing the array of argv
    printf("*(pcp + 1) = %s\n", *(pcp + 1)); // Indexing is the same as dereferencing a memory address
  }

  // Read backwards
  const char * pcc = c;  // pcc is a pointer to char constant
  char const * pcc2 = c;  // What is the type of pcc2? pointer to a const char. const can be on either side of a type, so same as above.
  printf("*pcc = %s\n", pcc); 
  char * pc3 = c; // Valid assignment: c acts like a pointer to c[0] here. i.e. &c[0]
  printf("pc3 = %s\n", pc3); // If you use %s, will print until null character (\0). Don't need to dereference.

  // For each of the following, why is the assignment:
  //*pcc = '7';  // invalid? Can't change const values (read-only)
  pcc = *pcp;  // valid? Changing where pointer points to. It is now a pointing to a char.
  printf("*pcc = %s\n", pcc); 
  pcc = argv[0];  // valid?  // Changing pointer
  printf("*pcc = %s\n", pcc); 

  char * const cp = c;  // cp is a const pointer to char
  // For each of the following, why is the assignment:
  //cp = *pcp;  // invalid? // Can't change the pointer. It is read-only
  //cp = *argv;  // invalid? // Can't change the pointer. It is read-only
  *cp = '!';  // valid? // Can change the character (after it is dereferenced). The first element's address is dereferenced.
  printf("cp = %s\n", cp); 
  *(cp + 1) = '?'; // Assign value to second element by dereferencing the address of the second element
  printf("cp = %s\n", cp); 

  const char * const cpc = c;  // cpc is a const pointer to char const
  // For each of the following, why is the assignment:
  //cpc = *pcp;  // invalid? // Can't change the pointer.
  //cpc = argv[0];  // invalid? // Can't change the pointer.
  //*cpc = '@';  // invalid? // Can't change the value.

  return 0;
}
