/*
 * cache.c
 * package object cache
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
 * libpkg-config `cache` module
 * ============================
 *
 * The libpkg-config `cache` module manages a package/module object cache,
 * allowing it to avoid loading duplicate copies of a package/module.
 *
 * A cache is tied to a specific pkg-config client object, so package objects
 * should not be shared across threads.
 */

/*
 * !doc
 *
 * .. c:function:: pkg_config_pkg_t *pkg_config_cache_lookup(const
 * pkg_config_client_t *client, const char *id)
 *
 *    Looks up a package in the cache given an `id` atom,
 *    such as ``gtk+-3.0`` and returns the already loaded version
 *    if present.
 *
 *    :param pkg_config_client_t* client: The client object to access.
 *    :param char* id: The package atom to look up in the client object's
 * cache. :return: A package object if present, else ``NULL``. :rtype:
 * pkg_config_pkg_t *
 */
pkg_config_pkg_t*
pkg_config_cache_lookup (pkg_config_client_t* client, const char* id)
{
  pkg_config_node_t* node;

  LIBPKG_CONFIG_FOREACH_LIST_ENTRY (client->pkg_cache.head, node)
  {
    pkg_config_pkg_t* pkg = node->data;

    if (!strcmp (pkg->id, id))
    {
      PKG_CONFIG_TRACE (client, "found: %s @%p", id, pkg);
      return pkg_config_pkg_ref (client, pkg);
    }
  }

  PKG_CONFIG_TRACE (client, "miss: %s", id);
  return NULL;
}

/*
 * !doc
 *
 * .. c:function:: void pkg_config_cache_add(pkg_config_client_t *client,
 * pkg_config_pkg_t *pkg)
 *
 *    Adds an entry for the package to the package cache.
 *    The cache entry must be removed if the package is freed.
 *
 *    :param pkg_config_client_t* client: The client object to modify.
 *    :param pkg_config_pkg_t* pkg: The package object to add to the client
 * object's cache. :return: nothing
 */
void
pkg_config_cache_add (pkg_config_client_t* client, pkg_config_pkg_t* pkg)
{
  if (pkg == NULL)
    return;

  pkg_config_pkg_ref (client, pkg);
  pkg_config_list_insert (&pkg->cache_iter, pkg, &client->pkg_cache);

  PKG_CONFIG_TRACE (client, "added @%p to cache", pkg);

  /* mark package as cached */
  pkg->flags |= LIBPKG_CONFIG_PKG_PROPF_CACHED;
}

/*
 * !doc
 *
 * .. c:function:: void pkg_config_cache_remove(pkg_config_client_t *client,
 * pkg_config_pkg_t *pkg)
 *
 *    Deletes a package from the client object's package cache.
 *
 *    :param pkg_config_client_t* client: The client object to modify.
 *    :param pkg_config_pkg_t* pkg: The package object to remove from the
 * client object's cache. :return: nothing
 */
void
pkg_config_cache_remove (pkg_config_client_t* client, pkg_config_pkg_t* pkg)
{
  if (pkg == NULL)
    return;

  if (!(pkg->flags & LIBPKG_CONFIG_PKG_PROPF_CACHED))
    return;

  PKG_CONFIG_TRACE (client, "removed @%p from cache", pkg);

  pkg_config_list_delete (&pkg->cache_iter, &client->pkg_cache);
}

static inline void
clear_dependency_matches (pkg_config_list_t* list)
{
  pkg_config_node_t* iter;

  LIBPKG_CONFIG_FOREACH_LIST_ENTRY (list->head, iter)
  {
    pkg_config_dependency_t* dep = iter->data;
    dep->match = NULL;
  }
}

/*
 * !doc
 *
 * .. c:function:: void pkg_config_cache_free(pkg_config_client_t *client)
 *
 *    Releases all resources related to a client object's package cache.
 *    This function should only be called to clear a client object's package
 * cache, as it may release any package in the cache.
 *
 *    :param pkg_config_client_t* client: The client object to modify.
 */
void
pkg_config_cache_free (pkg_config_client_t* client)
{
  pkg_config_node_t *iter, *iter2;

  /* first we clear cached match pointers */
  LIBPKG_CONFIG_FOREACH_LIST_ENTRY (client->pkg_cache.head, iter)
  {
    pkg_config_pkg_t* pkg = iter->data;

    clear_dependency_matches (&pkg->required);
    clear_dependency_matches (&pkg->requires_private);
    clear_dependency_matches (&pkg->conflicts);
  }

  /* now forcibly free everything */
  LIBPKG_CONFIG_FOREACH_LIST_ENTRY_SAFE (client->pkg_cache.head, iter2, iter)
  {
    pkg_config_pkg_t* pkg = iter->data;
    pkg_config_pkg_free (client, pkg);
  }

  memset (&client->pkg_cache, 0, sizeof client->pkg_cache);

  PKG_CONFIG_TRACE (client, "cleared package cache");
}
