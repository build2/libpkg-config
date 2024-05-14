/*
 * dependency.c
 * dependency parsing and management
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

#include <libpkg-config/pkg-config.h>

#include <libpkg-config/stdinc.h>

/*
 * !doc
 *
 * libpkg-config `dependency` module
 * =================================
 *
 * The `dependency` module provides support for building `dependency lists`
 * (the basic component of the overall `dependency graph`) and `dependency
 * nodes` which store dependency information.
 */

#define PKG_CONFIG_IS_MODULE_SEPARATOR(c)                                    \
  ((c) == ',' || isspace ((unsigned int)(c)))
#define PKG_CONFIG_IS_OPERATOR_CHAR(c)                                       \
  ((c) == '<' || (c) == '>' || (c) == '!' || (c) == '=')

typedef enum
{
  OUTSIDE_MODULE = 0,
  INSIDE_MODULE_NAME = 1,
  BEFORE_OPERATOR = 2,
  INSIDE_OPERATOR = 3,
  AFTER_OPERATOR = 4,
  INSIDE_VERSION = 5
} parse_state_t;

#define DEBUG_PARSE 0

#ifndef LIBPKG_CONFIG_NTRACE
static const char*
dependency_to_str (const pkg_config_dependency_t* dep,
                   char* buf,
                   size_t buflen)
{
  pkg_config_strlcpy (buf, dep->package, buflen);
  if (dep->version != NULL)
  {
    pkg_config_strlcat (buf, " ", buflen);
    pkg_config_strlcat (buf, pkg_config_pkg_get_comparator (dep), buflen);
    pkg_config_strlcat (buf, " ", buflen);
    pkg_config_strlcat (buf, dep->version, buflen);
  }

  return buf;
}
#endif

/* find a colliding dependency that is coloured differently */
static inline pkg_config_dependency_t*
find_colliding_dependency (const pkg_config_dependency_t* dep,
                           const pkg_config_list_t* list)
{
  const pkg_config_node_t* n;

  LIBPKG_CONFIG_FOREACH_LIST_ENTRY (list->head, n)
  {
    pkg_config_dependency_t* dep2 = n->data;

    if (strcmp (dep->package, dep2->package))
      continue;

    if (dep->flags != dep2->flags)
      return dep2;
  }

  return NULL;
}

static inline pkg_config_dependency_t*
add_or_replace_dependency_node (const pkg_config_client_t* client,
                                pkg_config_dependency_t* dep,
                                pkg_config_list_t* list)
{
#ifndef LIBPKG_CONFIG_NTRACE
  char depbuf[PKG_CONFIG_ITEM_SIZE];
#else
  (void)client;
#endif
  pkg_config_dependency_t* dep2 = find_colliding_dependency (dep, list);

  /* there is already a node in the graph which describes this dependency */
  if (dep2 != NULL)
  {
#ifndef LIBPKG_CONFIG_NTRACE
    char depbuf2[PKG_CONFIG_ITEM_SIZE];
#endif
    PKG_CONFIG_TRACE (client,
                      "dependency collision: [%s/%x] -- [%s/%x]",
                      dependency_to_str (dep, depbuf, sizeof depbuf),
                      dep->flags,
                      dependency_to_str (dep2, depbuf2, sizeof depbuf2),
                      dep2->flags);

    /* prefer the uncoloured node, either dep or dep2 */
    if (dep->flags && dep2->flags == 0)
    {
      PKG_CONFIG_TRACE (client,
                        "dropping dependency [%s]@%p because of collision",
                        depbuf,
                        dep);

      free (dep);
      return NULL;
    }
    else if (dep2->flags && dep->flags == 0)
    {
      PKG_CONFIG_TRACE (client,
                        "dropping dependency [%s]@%p because of collision",
                        depbuf2,
                        dep2);

      pkg_config_list_delete (&dep2->iter, list);
      free (dep2);
    }
    else
      /* If both dependencies have equal strength, we keep both, because of
       * situations like: Requires: foo > 1, foo < 3
       *
       * If the situation is that both dependencies are literally equal, it is
       * still harmless because fragment deduplication will handle the
       * excessive fragments.
       */
      PKG_CONFIG_TRACE (client, "keeping both dependencies (harmless)");
  }

  PKG_CONFIG_TRACE (client,
                    "added dependency [%s] to list @%p; flags=%x",
                    dependency_to_str (dep, depbuf, sizeof depbuf),
                    list,
                    dep->flags);
  pkg_config_list_insert_tail (&dep->iter, dep, list);

  return dep;
}

static inline pkg_config_dependency_t*
pkg_config_dependency_addraw (const pkg_config_client_t* client,
                              pkg_config_list_t* list,
                              const char* package,
                              size_t package_sz,
                              const char* version,
                              size_t version_sz,
                              pkg_config_pkg_comparator_t compare,
                              unsigned int flags)
{
  pkg_config_dependency_t* dep;

  dep = calloc (1, sizeof (pkg_config_dependency_t));
  dep->package = pkg_config_strndup (package, package_sz);

  if (version_sz != 0)
    dep->version = pkg_config_strndup (version, version_sz);

  dep->compare = compare;
  dep->flags = flags;

  return add_or_replace_dependency_node (client, dep, list);
}

/*
 * !doc
 *
 * .. c:function:: pkg_config_dependency_t
 * *pkg_config_dependency_add(pkg_config_list_t *list, const char *package,
 * const char *version, pkg_config_pkg_comparator_t compare)
 *
 *    Adds a parsed dependency to a dependency list as a dependency node.
 *
 *    :param pkg_config_client_t* client: The client object that owns the
 * package this dependency list belongs to. :param pkg_config_list_t* list:
 * The dependency list to add a dependency node to. :param char* package: The
 * package `atom` to set on the dependency node. :param char* version: The
 * package `version` to set on the dependency node. :param
 * pkg_config_pkg_comparator_t compare: The comparison operator to set on the
 * dependency node. :param uint flags: Any flags to attach to the dependency
 * node. :return: A dependency node. :rtype: pkg_config_dependency_t *
 */
pkg_config_dependency_t*
pkg_config_dependency_add (const pkg_config_client_t* client,
                           pkg_config_list_t* list,
                           const char* package,
                           const char* version,
                           pkg_config_pkg_comparator_t compare,
                           unsigned int flags)
{
  if (version != NULL)
    return pkg_config_dependency_addraw (client,
                                         list,
                                         package,
                                         strlen (package),
                                         version,
                                         strlen (version),
                                         compare,
                                         flags);

  return pkg_config_dependency_addraw (
      client, list, package, strlen (package), NULL, 0, compare, flags);
}

/*
 * !doc
 *
 * .. c:function:: void pkg_config_dependency_append(pkg_config_list_t *list,
 * pkg_config_dependency_t *tail)
 *
 *    Adds a dependency node to a pre-existing dependency list.
 *
 *    :param pkg_config_list_t* list: The dependency list to add a dependency
 * node to. :param pkg_config_dependency_t* tail: The dependency node to add
 * to the tail of the dependency list. :return: nothing
 */
void
pkg_config_dependency_append (pkg_config_list_t* list,
                              pkg_config_dependency_t* tail)
{
  pkg_config_list_insert_tail (&tail->iter, tail, list);
}

/*
 * !doc
 *
 * .. c:function:: void pkg_config_dependency_free(pkg_config_list_t *list)
 *
 *    Release a dependency list and it's child dependency nodes.
 *
 *    :param pkg_config_list_t* list: The dependency list to release.
 *    :return: nothing
 */
void
pkg_config_dependency_free (pkg_config_list_t* list)
{
  pkg_config_node_t *node, *next;

  LIBPKG_CONFIG_FOREACH_LIST_ENTRY_SAFE (list->head, next, node)
  {
    pkg_config_dependency_t* dep = node->data;

    if (dep->match != NULL)
      pkg_config_pkg_unref (dep->match->owner, dep->match);

    if (dep->package != NULL)
      free (dep->package);

    if (dep->version != NULL)
      free (dep->version);

    free (dep);
  }
}

/*
 * !doc
 *
 * .. c:function:: void pkg_config_dependency_parse_str(pkg_config_list_t
 * *deplist_head, const char *depends)
 *
 *    Parse a dependency declaration into a dependency list.
 *    Commas are counted as whitespace to allow for constructs such as
 * ``@SUBSTVAR@, zlib`` being processed into ``, zlib``.
 *
 *    :param pkg_config_client_t* client: The client object that owns the
 * package this dependency list belongs to. :param pkg_config_list_t*
 * deplist_head: The dependency list to populate with dependency nodes. :param
 * char* depends: The dependency data to parse. :param uint flags: Any flags
 * to attach to the dependency nodes. :return: nothing
 */
void
pkg_config_dependency_parse_str (const pkg_config_client_t* client,
                                 pkg_config_list_t* deplist_head,
                                 const char* depends,
                                 unsigned int flags)
{
  parse_state_t state = OUTSIDE_MODULE;
  pkg_config_pkg_comparator_t compare = PKG_CONFIG_CMP_ANY;
  char buf[PKG_CONFIG_BUFSIZE];
  size_t package_sz = 0, version_sz = 0;
  char* start = buf;
  char* ptr = buf;
  char* vstart = NULL;
  char *package = NULL, *version = NULL;
  char cmpname[32]; /* One of pkg_config_pkg_comparator_names. */
  char* cnameptr = cmpname;
  char* cnameend = cmpname + 32 - 1;

  memset (cmpname, '\0', sizeof cmpname);

  pkg_config_strlcpy (buf, depends, sizeof buf);
  pkg_config_strlcat (buf, " ", sizeof buf);

  while (*ptr)
  {
    switch (state)
    {
    case OUTSIDE_MODULE:
      if (!PKG_CONFIG_IS_MODULE_SEPARATOR (*ptr))
        state = INSIDE_MODULE_NAME;

      break;

    case INSIDE_MODULE_NAME:
      if (isspace ((unsigned int)*ptr))
      {
        const char* sptr = ptr;

        while (*sptr && isspace ((unsigned int)*sptr)) sptr++;

        if (*sptr == '\0')
          state = OUTSIDE_MODULE;
        else if (PKG_CONFIG_IS_MODULE_SEPARATOR (*sptr))
          state = OUTSIDE_MODULE;
        else if (PKG_CONFIG_IS_OPERATOR_CHAR (*sptr))
          state = BEFORE_OPERATOR;
        else
          state = OUTSIDE_MODULE;
      }
      else if (PKG_CONFIG_IS_MODULE_SEPARATOR (*ptr))
        state = OUTSIDE_MODULE;
      else if (*(ptr + 1) == '\0')
      {
        ptr++;
        state = OUTSIDE_MODULE;
      }

      if (state != INSIDE_MODULE_NAME && start != ptr)
      {
        char* iter = start;

        while (PKG_CONFIG_IS_MODULE_SEPARATOR (*iter)) iter++;

        package = iter;
        package_sz = ptr - iter;
        start = ptr;
      }

      if (state == OUTSIDE_MODULE)
      {
        pkg_config_dependency_addraw (client,
                                      deplist_head,
                                      package,
                                      package_sz,
                                      NULL,
                                      0,
                                      compare,
                                      flags);

        compare = PKG_CONFIG_CMP_ANY;
        package_sz = 0;
      }

      break;

    case BEFORE_OPERATOR:
      if (PKG_CONFIG_IS_OPERATOR_CHAR (*ptr))
      {
        state = INSIDE_OPERATOR;
        if (cnameptr < cnameend)
          *cnameptr++ = *ptr;
      }

      break;

    case INSIDE_OPERATOR:
      if (!PKG_CONFIG_IS_OPERATOR_CHAR (*ptr))
      {
        state = AFTER_OPERATOR;
        compare = pkg_config_pkg_comparator_lookup_by_name (cmpname);
      }
      else if (cnameptr < cnameend)
        *cnameptr++ = *ptr;

      break;

    case AFTER_OPERATOR:
      if (!isspace ((unsigned int)*ptr))
      {
        vstart = ptr;
        state = INSIDE_VERSION;
      }
      break;

    case INSIDE_VERSION:
      if (PKG_CONFIG_IS_MODULE_SEPARATOR (*ptr) || *(ptr + 1) == '\0')
      {
        version = vstart;
        version_sz = ptr - vstart;
        state = OUTSIDE_MODULE;

        pkg_config_dependency_addraw (client,
                                      deplist_head,
                                      package,
                                      package_sz,
                                      version,
                                      version_sz,
                                      compare,
                                      flags);

        compare = PKG_CONFIG_CMP_ANY;
        cnameptr = cmpname;
        memset (cmpname, 0, sizeof cmpname);
        package_sz = 0;
      }

      if (state == OUTSIDE_MODULE)
        start = ptr;
      break;
    }

    ptr++;
  }
}

/*
 * !doc
 *
 * .. c:function:: void pkg_config_dependency_parse(const pkg_config_client_t
 * *client, pkg_config_pkg_t *pkg, pkg_config_list_t *deplist, const char
 * *depends)
 *
 *    Preprocess dependency data and then process that dependency declaration
 * into a dependency list. Commas are counted as whitespace to allow for
 * constructs such as ``@SUBSTVAR@, zlib`` being processed into ``, zlib``.
 *
 *    :param pkg_config_client_t* client: The client object that owns the
 * package this dependency list belongs to. :param pkg_config_pkg_t* pkg: The
 * package object that owns this dependency list. :param pkg_config_list_t*
 * deplist: The dependency list to populate with dependency nodes. :param
 * char* depends: The dependency data to parse. :param uint flags: Any flags
 * to attach to the dependency nodes. :return: nothing
 */
void
pkg_config_dependency_parse (const pkg_config_client_t* client,
                             pkg_config_pkg_t* pkg,
                             pkg_config_list_t* deplist,
                             const char* depends,
                             unsigned int flags)
{
  char* kvdepends = pkg_config_tuple_parse (client, &pkg->vars, depends);

  pkg_config_dependency_parse_str (client, deplist, kvdepends, flags);
  free (kvdepends);
}
