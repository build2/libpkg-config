/*
 * list.h
 * Linked lists.
 *
 * ISC License
 *
 * Copyright (c) the build2 authors (see the COPYRIGHT, AUTHORS files).
 * Copyright (c) 2011-2018 pkgconf authors (see AUTHORS file).
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef LIBPKG_CONFIG_LIST_H
#define LIBPKG_CONFIG_LIST_H

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct pkg_config_node_ pkg_config_node_t;

struct pkg_config_node_
{
  pkg_config_node_t *prev, *next;
  void* data;
};

typedef struct
{
  pkg_config_node_t *head, *tail;
  size_t length;
} pkg_config_list_t;

#define LIBPKG_CONFIG_LIST_INITIALIZER {NULL, NULL, 0}

#define LIBPKG_CONFIG_FOREACH_LIST_ENTRY(head, value)                       \
  for ((value) = (head); (value) != NULL; (value) = (value)->next)

#define LIBPKG_CONFIG_FOREACH_LIST_ENTRY_SAFE(head, nextiter, value)        \
  for ((value) = (head), (nextiter) = (head) != NULL ? (head)->next : NULL; \
       (value) != NULL;                                                     \
       (value) = (nextiter),                                                \
         (nextiter) = (nextiter) != NULL ? (nextiter)->next : NULL)

#define LIBPKG_CONFIG_FOREACH_LIST_ENTRY_REVERSE(tail, value)               \
  for ((value) = (tail); (value) != NULL; (value) = (value)->prev)

static inline void
pkg_config_list_zero (pkg_config_list_t* list)
 {
  list->head = NULL;
  list->tail = NULL;
  list->length = 0;
}

static inline void
pkg_config_list_insert (pkg_config_node_t* node,
                        void* data,
                        pkg_config_list_t* list)
{
  pkg_config_node_t* tnode;

  node->data = data;

  if (list->head == NULL)
  {
    list->head = node;
    list->tail = node;
    list->length = 1;
    return;
  }

  tnode = list->head;

  node->next = tnode;
  tnode->prev = node;

  list->head = node;
  list->length++;
}

static inline void
pkg_config_list_insert_tail (pkg_config_node_t* node,
                             void* data,
                             pkg_config_list_t* list)
{
  pkg_config_node_t* tnode;

  node->data = data;

  if (list->tail == NULL)
  {
    list->head = node;
    list->tail = node;
    list->length = 1;
    return;
  }

  tnode = list->tail;

  node->prev = tnode;
  tnode->next = node;

  list->tail = node;
  list->length++;
}

static inline void
pkg_config_list_delete (pkg_config_node_t* node, pkg_config_list_t* list)
{
  list->length--;

  if (node->prev == NULL)
    list->head = node->next;
  else
    node->prev->next = node->next;

  if (node->next == NULL)
    list->tail = node->prev;
  else
    node->next->prev = node->prev;
}

#ifdef __cplusplus
}
#endif

#endif /* LIBPKG_CONFIG_LIST_H */
