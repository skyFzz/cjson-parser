#include <stdlib.h>
#include <stdio.h>
#include "dynamic_array.h"

#define sys_error(sys) \
  do { perror(sys); exit(EXIT_FAILURE); } while (0);

void push_back(dynamic_array *arr, void *element) {
  if (arr->size == arr->capacity) {
    arr->capacity *= 2;
    if ((arr->elements = realloc(arr->elements, sizeof(void *) * arr->capacity)) == NULL)
      sys_error("malloc");
  } 
  *(arr->elements + arr->size) = element;
  arr->size++;
}

void pop_back(dynamic_array *arr) {
  *(arr->elements + arr->size) = NULL;
}
