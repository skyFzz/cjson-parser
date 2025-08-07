/**
 * map.c
 *
 * A custum unordered map implementation using hash_table universal hashing for
 * the confidential inference project.
 *
 * Author: Haoling Zhou
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "map.h"

//#define DEBUG
#define PRIME 101
#define A 53
#define B 79

int hash(char *key, int key_len, int table_size) {
  int key_val = 0;

  for (int i = 0; i < key_len; i++) {
    key_val += key[i];
  } 

  return ((A * key_val + B) % PRIME) % table_size;
}

void insert(chain_node *node, map *ht) { 
  int idx = hash(node->key, strlen(node->key), ht->size);

#ifdef DEBUG
  printf("node key: %s\n", node->key); 
  printf("hash value: %d\n", idx); 
#endif 

  if (idx < 0 || idx >= ht->size) {
    printf("Error: idx out of bound\n");
    exit(EXIT_FAILURE);
  }

  if (ht->lists[idx]->head == NULL) {
    ht->lists[idx]->head = node;
  } else {
    chain_node *tmp = ht->lists[idx]->head;
    ht->lists[idx]->head = node;
    node->next = tmp;
    tmp->prev = node;
  }

#ifdef DEBUG
  printf("New head node key: %s\n", ht->lists[idx]->head->key); 
  if (ht->lists[idx]->head->next != NULL)
    printf("Old head node key: %s\n", ht->lists[idx]->head->next->key); 
#endif 
}

void *search(char *key, map *ht) {
  int idx = hash(key, strlen(key), ht->size); 

  chain_node *cur = ht->lists[idx]->head;
  while (strcmp(key, cur->key) != 0 && cur != NULL)
    cur = cur->next;

  if (cur == NULL)
    return NULL;
  
  return cur->val;
}
