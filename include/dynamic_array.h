#define DYNAMIC_ARRAY_H
#ifdef DYNAMIC_ARRAY_H
typedef struct {
  void **elements; 
  int size;
  int capacity;
} dynamic_array;

void push_back(dynamic_array *arr, void *element);
void pop_back(dynamic_array *arr);
#endif
