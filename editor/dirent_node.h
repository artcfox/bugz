#pragma once

#include <dirent.h>
#include <string.h>

#include "rbtree/rbtree.h"
#include "rbtree/rbtree+setinsert.h"

typedef struct _dirent_node_t { rbtree_node_t super;
  struct dirent entry;
} dirent_node_t;

rbtree_node_t *dirent_node_new(struct dirent *entry) {
  dirent_node_t *self = malloc(sizeof(dirent_node_t));
  memcpy(&self->entry, entry, sizeof(self->entry));
  return (rbtree_node_t *)self;
}

void dirent_node_delete(rbtree_node_t *self, void *context) {
  free(self);
  self = NULL;
}

int dirent_node_compare(const rbtree_node_t *x, const rbtree_node_t *y) {
  return strcmp(((const dirent_node_t *)x)->entry.d_name,
		((const dirent_node_t *)y)->entry.d_name);
}
