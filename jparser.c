/**
 * jparser.c
 *
 * Author: Haoling Zhou
 * 
 * A custom JSON parser for the confidential inference project.
 * This implementation does not parse unicode.
 *
 * July 2025
 */
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "ordered_map.h"

#define sys_error(sys) \
  do { perror(sys); exit(EXIT_FAILURE); } while (0)
#define parse_error(msg) \
  do { printf(msg); exit(EXIT_FAILURE); } while (0)
  
#define PAGE 4096
#define ASCII_MAX 127
#define MAX_KEY_LEN 100   // enough for confidential inference
#define SIZE_ORDERED_MAP 20
#define SIZE_DYNAMIC_ARRAY 10 
#define SIZE_SHORT_STRING 50
#define SIZE_NUM_STRING 100 

#define TEST

typedef enum {
  TYPE_OBJECT,
  TYPE_ARRAY,
  TYPE_STRING,
  TYPE_NULL,
  TYPE_NUMBER,
  TYPE_BOOL
} json_data_type;

/**
 * The null data type does not need space.
 */
typedef union {
  ordered_map *obj;      // object 
  dynamic_array *arr;   // array
  char *str;            // string
  char *num;           // number
  bool boo;             // true | false 
} json_data;

typedef struct {
  json_data *data;
  json_data_type type;
  int layer;
} json_text;

json_text *parse_any(char *buf, int *idx, int size, int layer);

size_t hash_func(char *key, size_t table_size) {
  size_t idx;

  for (char *s = key; *s != '\0'; s++)
    idx += *s;

  if (idx >= table_size)
    idx %= table_size; 

  return idx;
}

json_text *construct_json_text(int layer) {
  json_text *res = (json_text *)malloc(sizeof(json_text));
  res->data = (json_data *)malloc(sizeof(json_data));
  res->type = -1;
  res->layer = layer;

  return res;
}

ordered_map *construct_ordered_map() {
  ordered_map *res = (ordered_map *)malloc(sizeof(ordered_map));

  res->size = SIZE_ORDERED_MAP;
  res->lists = (chain_list **)malloc(sizeof(chain_list *) * SIZE_ORDERED_MAP);
  res->node_list = (dynamic_array *)malloc(sizeof(dynamic_array));

  res->node_list->size = 0;
  res->node_list->capacity = 30;  // initial total number of keys
  res->node_list->elements = (void **)malloc(sizeof(void *) * 30);

  for (int i = 0; i < SIZE_ORDERED_MAP; i++) {
    res->lists[i] = (chain_list *)malloc(sizeof(chain_list));
    res->lists[i]->head = NULL;
  }

  return res;
}

chain_node *construct_chain_node() {
  chain_node *node = (chain_node *)malloc(sizeof(chain_node)); 
  return node;
}

dynamic_array *construct_dynamic_array() {
  dynamic_array *new_arr = (dynamic_array *)malloc(sizeof(dynamic_array));

  new_arr->size = 0;
  new_arr->capacity = SIZE_DYNAMIC_ARRAY; 
  new_arr->elements = (void **)malloc(sizeof(void *) * SIZE_DYNAMIC_ARRAY);
  
  return new_arr;
}

/**
 * free_json_text - recursively free the JSON text
 * @jtext: root JSON text
 */
void free_json_text(json_text *jtext) {
  json_data *data = jtext->data;

  switch (jtext->type) {
    case TYPE_STRING: 
      free(data->str);
      break;
    case TYPE_NUMBER: 
      free(data->num);
      break;
    case TYPE_ARRAY: 
      for (int i = 0; i < data->arr->size; i++) {
        free_json_text((json_text *)data->arr->elements[i]);    // recursive free
      }
      free(data->arr->elements);
      free(data->arr);
      break;
    case TYPE_OBJECT: 
      for (int i = 0; i < data->obj->node_list->size; i++) {
        chain_node *node = (chain_node *)data->obj->node_list->elements[i];
        free(node->prev);  
        free(node->next);  
        free(node->key);
        free_json_text((json_text *)node->val);     // recursive free
        free(node);
      }
      free(data->obj->node_list);

      for (int j = 0; j < data->obj->size; j++) {
        /**
         * free the obj->lists[j]->head here would be double-free because head
         * is not a dummy node. Head is already freed as a chain node.
         */
        free(data->obj->lists[j]);
      } 
      free(data->obj->lists); 

      free(data->obj);
      break;
    default:
  }
  free(data);
  free(jtext);
}

bool is_digit(char c) {
  return c - '0' >= 0 && c - '9' <= 9;
}

bool is_whitespace_char(char c) {
  return c == ' ' || c == 'b' || c == 'f' || 
         c == 'n' || c == 'r' || c == 't';
}

bool is_whitespace(char c) {
  return c == ' ' || c == '\b' || c == '\f' || 
         c == '\n' || c == '\r' || c == '\t';
}

void parse_whitespace(char *buf, int *idx, int size) {
  while (buf[*idx] == ' ' || buf[*idx] == '\t' || buf[*idx] == '\n' || buf[*idx] == '\f' || buf[*idx] == '\r' || buf[*idx] == '\v')
    (*idx)++;
}

/**
 * parse_num - parse a valid number
 * @buf: original plaintext
 * @idx: index of the character which should be examined next
 * @size: size of @buf, used to bound the search
 */
char *parse_num(char *buf, int *idx, int size) {
  char *new_num = (char *)malloc(sizeof(char) * SIZE_NUM_STRING);
  int i = 0;
 
  while (!is_whitespace(buf[*idx]) && buf[*idx] != ',' && buf[*idx] != '}' && buf[*idx] != ']' && i < size) {
    if (!is_digit(buf[*idx])) {
      printf("buf[*idx]: %c\n", buf[*idx]);
      parse_error("parse_num: invalid number\n");
    }
    new_num[i] = buf[*idx];
    i++;
    (*idx)++;
  }

  if (is_digit(buf[*idx]) || i == size)
    parse_error("parse_num: buffer overflow\n"); 

  new_num[i] = '\0';

  return new_num;
}

/**
 * parse_null - parse a null string
 *
 * A normal return indicates a valid null string.
 */
void parse_null(char *buf, int *idx, int size) {
  size_t size_null_str = 4;
  if (strncmp(buf + *idx, "null", size_null_str) != 0) {
    parse_error("parse_null: undefined null string\n");
  }
  (*idx) += size_null_str;
}

/**
 * parse_bool - parse a boolean string
 * @boo_type: show whether looking for "true" or "false" string
 */
void parse_bool(char *buf, int *idx, int size, bool boo_type) {
  size_t size_true_str = 4;
  size_t size_false_str = 5;

  if (boo_type) {
    if (strncmp(buf + *idx, "true", size_true_str) != 0) {
      parse_error("parse_bool: undefined \"true\" boolean string\n");
    }
    (*idx) += size_true_str;
  } else {
    if (strncmp(buf + *idx, "false", size_false_str) != 0) {
      parse_error("parse_bool: undefined \"false\" boolean string\n");
    }
    (*idx) += size_false_str;
  }
}

/**
 * parse_short_string - parse and return a valid short-size string
 */
char *parse_short_string(char *buf, int *idx, int size) {
  char *str = (char *)malloc(sizeof(char) * SIZE_SHORT_STRING);
  int i = 0;
  
  while (buf[*idx] != '\"' && *idx < size && i < SIZE_SHORT_STRING) {
    if (buf[*idx] == '\\') {
      str[i] = buf[*idx]; // store the backslash too and increment indicies
      i++;      
      (*idx)++; 
      if (*idx < size && (is_whitespace_char(buf[*idx]) || buf[*idx] == '/' || buf[*idx] == '\\' || buf[*buf] == '\"')) {
        str[i] = buf[*idx];
      } else {
        printf("Parsing error: invald escape sequence\n");
        exit(EXIT_FAILURE);
      }
    } else {
      str[i] = buf[*idx];
    }
    i++;
    (*idx)++;
  }
  
  if (buf[*idx] != '\"') {
    printf("parse_short_string: invald string or buffer overflow\n");
    exit(EXIT_FAILURE);
  }

  str[i] = '\0';
  (*idx)++;
  return str;
}

/**
 * parse_element - helper which recursively fills the JSON array
 * @arr: the dynamic array which stores the elements of an JSON data type
 */
void *parse_element(char *buf, int *idx, int size, dynamic_array *arr, int layer) {
  json_text *new_jtext;

  new_jtext = parse_any(buf, idx, size, layer);
  push_back(arr, (void *)new_jtext);
  parse_whitespace(buf, idx, size);

  if (buf[*idx] == ',') {
    (*idx)++;
    parse_whitespace(buf, idx, size);
    parse_element(buf, idx, size, arr, layer);
  }
}

/**
 * parse_array - parse and return a valid array
 */
dynamic_array *parse_array(char *buf, int *idx, int size, int layer) {
  dynamic_array *new_arr;
  new_arr = construct_dynamic_array();

  parse_element(buf, idx, size, new_arr, layer + 1);

  if (buf[*idx] != ']') {
    parse_error("parse_array: miss closing square bracket\n"); 
  }
  (*idx)++;
  
  return new_arr; 
}

/**
 * parse_node - helper which recursively fills the JSON object 
 * @map: the dictionary in which node will be inserted
 */
void parse_node(char *buf, int *idx, int size, ordered_map *map, int layer) {
  chain_node *new_node;
  char *key;
  json_text *val; 
  
  if (buf[*idx] != '\"')
    parse_error("parse_object: key must be a valid string\n");
  (*idx)++;

  key = parse_short_string(buf, idx, size);
  parse_whitespace(buf, idx, size);

  if (buf[*idx] != ':')
    parse_error("parse_object: miss colon after a key\n"); 
  (*idx)++;

  parse_whitespace(buf, idx, size);
  val = parse_any(buf, idx, size, layer + 1); 

  new_node = construct_chain_node();
  new_node->key = key;
  new_node->val = val;
  insert(new_node, map);
  parse_whitespace(buf, idx, size);

  if (buf[*idx] == ',') {
    (*idx)++;
    parse_whitespace(buf, idx, size);
    parse_node(buf, idx, size, map, layer);
  }
}

/**
 * parse_object - parse text and return a map 
 */
ordered_map *parse_object(char *buf, int *idx, int size, int layer) {
  ordered_map *res;
  res = construct_ordered_map();

  parse_node(buf, idx, size, res, layer);

  //printf("buf[*idx]: %c\n", buf[*idx]);
  if (buf[*idx] != '}') {
    parse_error("parse_object: miss closing curly bracket or miss comma separator\n");
  } 
 
  return res;
}

/**
 * parse_any - process and return an arbitrary JSON data
 * 
 * Result can be one of the six data types:
 * dictionary, array, stirng, number, null, bool (true/false).
 */
json_text *parse_any(char *buf, int *idx, int size, int layer) {
  json_text *jtext; 
  jtext = construct_json_text(layer);

  parse_whitespace(buf, idx, size);

  switch (buf[*idx]) {
    case '{':
      (*idx)++;
      parse_whitespace(buf, idx, size);
      ordered_map *new_obj = parse_object(buf, idx, size, layer);
      jtext->type = TYPE_OBJECT;
      jtext->data->obj = new_obj;
      break;
    case '[':
      (*idx)++;
      parse_whitespace(buf, idx, size);
      dynamic_array *new_arr = parse_array(buf, idx, size, layer);
      jtext->type = TYPE_ARRAY;
      jtext->data->arr = new_arr;
      break;
    case '"':
      (*idx)++;
      char *new_str = parse_short_string(buf, idx, size); 
      jtext->type = TYPE_STRING;
      jtext->data->str = new_str;
      break;
    case 'n':
      parse_null(buf, idx, size);
      jtext->type = TYPE_NULL;
      jtext->data = NULL;
      break;
    case 't':
      parse_bool(buf, idx, size, true);
      jtext->type = TYPE_BOOL;
      jtext->data->boo = true;
      break;
    case 'f':
      parse_bool(buf, idx, size, false);
      jtext->type = TYPE_BOOL;
      jtext->data->boo = false;
      break;
    default:
      if (is_digit(buf[*idx])) {
        char *new_num = parse_num(buf, idx, size);
        jtext->type = TYPE_NUMBER;
        jtext->data->num = new_num;
      } else {
        parse_error("parse_any: invalid first character\n");
      }
  }

  return jtext;
}

/**
 * print_json_oneline - print a one-line JSON text
 * @jtext: a valid JSON object
 */
void print_json_oneline(json_text *jtext) {
  json_data *data = jtext->data;

  switch (jtext->type) {
    case TYPE_OBJECT:
      printf("{");
      /** 
       * Iterate through the node list which is guaranteed to have valid KV
       * pairs instead of the entire map.
       */
      for (int i = 0; i < data->obj->node_list->size; i++) {
        printf("\"%s\"", ((chain_node *)data->obj->node_list->elements[i])->key); 
        printf(":");
        print_json_oneline((json_text *)((chain_node *)data->obj->node_list->elements[i])->val);
        if (i < data->obj->node_list->size - 1) {
          printf(",");
        }
      }
      printf("}");
      break;
    case TYPE_ARRAY:
      printf("[");  
      for (int i = 0; i < data->arr->size; i++) {
        print_json_oneline(data->arr->elements[i]);
        if (i < data->arr->size - 1) {
          printf(",");
        }
      }
      printf("]");  
      break;
    case TYPE_STRING:
      printf("\"%s\"", data->str);
      break;
    case TYPE_NULL:
      printf("null");
      break;
    case TYPE_NUMBER:
      printf("%s", data->num);
      break;
    case TYPE_BOOL:
      if (data->boo) {
        printf("true");
      } else {
        printf("false");
      }
      break;
  }
}

/**
 * print_json_format - print formatted JSON text
 * @jtext: a valid JSON object
 */
void print_json_format(json_text *jtext) {
  json_data *data = jtext->data;

  switch (jtext->type) {
    case TYPE_OBJECT:
      printf("{\n");
      for (int i = 0; i < data->obj->node_list->size; i++) {
        for (int j = 0; j < jtext->layer + 1; j++) {    // elements need indentation
          printf("\t");
        }
        printf("\"%s\"", ((chain_node *)data->obj->node_list->elements[i])->key); 
        printf(":");
        print_json_format((json_text *)((chain_node *)data->obj->node_list->elements[i])->val);
        if (i < data->obj->node_list->size - 1) {
          printf(",");
        }
        printf("\n");
      }
      for (int j = 0; j < jtext->layer; j++) {          // closing bracket does not need indentation
        printf("\t");
      }
      printf("}");
      break;
    case TYPE_ARRAY:
      printf("[\n");  
      for (int i = 0; i < data->arr->size; i++) {
        for (int j = 0; j < ((json_text *)data->arr->elements[0])->layer; j++) {
          printf("\t");
        }
        print_json_format(data->arr->elements[i]);
        if (i < data->arr->size - 1) {
          printf(",");
        }
        printf("\n");
      }
      for (int j = 0; j < ((json_text *)data->arr->elements[0])->layer - 1; j++) {
        printf("\t");
      }
      printf("]");  
      break;
    case TYPE_STRING:
      printf("\"%s\"", data->str);
      break;
    case TYPE_NULL:
      printf("null");
      break;
    case TYPE_NUMBER:
      printf("%s", data->num);
      break;
    case TYPE_BOOL:
      if (data->boo) {
        printf("true");
      } else {
        printf("false");
      }
      break;
  }
}

/**
 * main - read a json file and fill a json_text object
 */
void main(int argc, char **argv) {
  if (argc != 2) {
    printf("Usage: ./jparser [json_file]\n");
    exit(EXIT_FAILURE);
  }

  int fd;
  if ((fd = open(argv[1], O_RDONLY)) == -1)
    sys_error("open"); 

  char buf[PAGE];
  json_text *jtext;
  int *idx = (int *)malloc(sizeof(int));
  *idx = 0;
  int numb;
  while ((numb = read(fd, buf, PAGE)) > 0) {
    jtext = parse_any(buf, idx, numb, 0); 
  }
  if (numb < 0)
    sys_error("read");

  close(fd);

  print_json_format(jtext);
  printf("\n");

  free_json_text(jtext);
}
