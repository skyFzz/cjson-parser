#define ORDERED_MAP
#ifdef ORDERED_MAP
#include "dynamic_array.h"
typedef struct chain_node {
  struct chain_node *prev, *next; 
  char *key;
  void *val;
} chain_node;

typedef struct chain_list {
  chain_node *head;
} chain_list;

typedef struct {
  chain_list **lists;
  dynamic_array *node_list;
  int size;
} ordered_map;

void insert(chain_node *node, ordered_map *map);
void *search(char *key, ordered_map *map);
#endif
