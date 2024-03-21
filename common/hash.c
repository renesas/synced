/**
 * @file hash.c
 * @brief Implements a simple hash table.
 * @note Copyright (C) 2015 Richard Cochran <richardcochran@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
/********************************************************************************************************************
* Release Tag: 2-0-5
* Pipeline ID: 310964
* Commit Hash: b166f770
********************************************************************************************************************/

#include <stdlib.h>
#include <string.h>

#include "hash.h"

#define HASH_TABLE_SIZE   100

struct node {
  char *key;
  void *data;
  struct node *next;
};

struct hash {
  struct node *table[HASH_TABLE_SIZE];
};

static unsigned int hash_function(const char* s)
{
  unsigned int i;

  for(i = 0; *s; s++) {
    i = 137 * i + *s;
  }

  return i % HASH_TABLE_SIZE;
}

struct hash *hash_create(void)
{
  struct hash *ht = calloc(1, sizeof(*ht));

  return ht;
}

void hash_destroy(struct hash *ht, void (*func)(void *))
{
  unsigned int i;
  struct node *n;
  struct node *next;
  struct node **table = ht->table;

  for(i = 0; i < HASH_TABLE_SIZE; i++) {
    for(n = table[i]; n; n = next) {
      next = n->next;
      if(func) {
        func(n->data);
      }
      free(n->key);
      free(n);
    }
  }

  free(ht);
}

int hash_insert(struct hash *ht, const char* key, void *data)
{
  unsigned int h;
  struct node *n;
  struct node **table = ht->table;

  h = hash_function(key);

  for(n = table[h]; n; n = n->next) {
    /* Encountered collison */
    if(!strcmp(n->key, key)) {
      /* Reject duplicate keys */
      return -1;
    /* Keys are not the same, so n = n->next, where if n->next is NULL, then exit for loop (separate chaining) */
    }
  }

  n = calloc(1, sizeof(*n));

  if(!n) {
    return -1;
  }

  n->key = strdup(key);

  if(!n->key) {
    free(n);
    return -1;
  }

  n->data = data;
  n->next = table[h];
  table[h] = n;

  return 0;
}

void *hash_lookup(struct hash *ht, const char* key)
{
  unsigned int h;
  struct node *n;
  struct node **table = ht->table;

  h = hash_function(key);

  for(n = table[h]; n; n = n->next) {
    if(!strcmp(n->key, key)) {
      return n->data;
    }
  }

  return NULL;
}
