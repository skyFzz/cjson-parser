#define MAP_H
#ifdef MAP_H
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
  int size;
} map;

void insert(chain_node *node, map *ht);
void *search(char *key, map *ht);
#endif
