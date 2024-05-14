/*
 * tuple.c
 * management of key->value tuples
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
 * libpkg-config `tuple` module
 * ============================
 *
 * The `tuple` module provides key-value mappings backed by a linked list. The
 * key-value mapping is mainly used for variable substitution when parsing .pc
 * files.
 *
 * There are two sets of mappings: a ``pkg_config_pkg_t`` specific mapping,
 * and a `global` mapping. The `tuple` module provides convenience wrappers
 * for managing the `global` mapping, which is attached to a given client
 * object.
 */

/*
 * !doc
 *
 * .. c:function:: void pkg_config_tuple_add_global(pkg_config_client_t
 * *client, const char *key, const char *value)
 *
 *    Defines a global variable, replacing the previous declaration if one was
 * set.
 *
 *    :param pkg_config_client_t* client: The pkg-config client object to
 * modify. :param char* key: The key for the mapping (variable name). :param
 * char* value: The value for the mapped entry. :return: nothing
 */
void
pkg_config_tuple_add_global (pkg_config_client_t* client,
                             const char* key,
                             const char* value)
{
  pkg_config_tuple_add (client, &client->global_vars, key, value, false);
}

/*
 * !doc
 *
 * .. c:function:: void pkg_config_tuple_find_global(const pkg_config_client_t
 * *client, const char *key)
 *
 *    Looks up a global variable.
 *
 *    :param pkg_config_client_t* client: The pkg-config client object to
 * access. :param char* key: The key or variable name to look up. :return: the
 * contents of the variable or ``NULL`` :rtype: char *
 */
char*
pkg_config_tuple_find_global (const pkg_config_client_t* client,
                              const char* key)
{
  pkg_config_node_t* node;

  LIBPKG_CONFIG_FOREACH_LIST_ENTRY (client->global_vars.head, node)
  {
    pkg_config_tuple_t* tuple = node->data;

    if (!strcmp (tuple->key, key))
      return tuple->value;
  }

  return NULL;
}

/*
 * !doc
 *
 * .. c:function:: void pkg_config_tuple_free_global(pkg_config_client_t
 * *client)
 *
 *    Delete all global variables associated with a pkg-config client object.
 *
 *    :param pkg_config_client_t* client: The pkg-config client object to
 * modify. :return: nothing
 */
void
pkg_config_tuple_free_global (pkg_config_client_t* client)
{
  pkg_config_tuple_free (&client->global_vars);
}

/*
 * !doc
 *
 * .. c:function:: void pkg_config_tuple_define_global(pkg_config_client_t
 * *client, const char *kv)
 *
 *    Parse and define a global variable.
 *
 *    :param pkg_config_client_t* client: The pkg-config client object to
 * modify. :param char* kv: The variable in the form of ``key=value``.
 *    :return: nothing
 */
void
pkg_config_tuple_define_global (pkg_config_client_t* client, const char* kv)
{
  char* workbuf = strdup (kv);
  char* value;

  value = strchr (workbuf, '=');
  if (value == NULL)
    goto out;

  *value++ = '\0';
  pkg_config_tuple_add_global (client, workbuf, value);
out:
  free (workbuf);
}

static void
pkg_config_tuple_find_delete (pkg_config_list_t* list, const char* key)
{
  pkg_config_node_t *node, *next;

  LIBPKG_CONFIG_FOREACH_LIST_ENTRY_SAFE (list->head, next, node)
  {
    pkg_config_tuple_t* tuple = node->data;

    if (!strcmp (tuple->key, key))
    {
      pkg_config_tuple_free_entry (tuple, list);
      return;
    }
  }
}

static char*
dequote (const char* value)
{
  char* buf = calloc ((strlen (value) + 1) * 2, 1);
  char* bptr = buf;
  const char* i;
  char quote = 0;

  if (*value == '\'' || *value == '"')
    quote = *value;

  for (i = value; *i != '\0'; i++)
  {
    if (*i == '\\' && quote && *(i + 1) == quote)
    {
      i++;
      *bptr++ = *i;
    }
    else if (*i != quote)
      *bptr++ = *i;
  }

  return buf;
}

/*
 * !doc
 *
 * .. c:function:: pkg_config_tuple_t *pkg_config_tuple_add(const
 * pkg_config_client_t *client, pkg_config_list_t *list, const char *key,
 * const char *value, bool parse)
 *
 *    Optionally parse and then define a variable.
 *
 *    :param pkg_config_client_t* client: The pkg-config client object to
 * access. :param pkg_config_list_t* list: The variable list to add the new
 * variable to. :param char* key: The name of the variable being added. :param
 * char* value: The value of the variable being added. :param bool parse:
 * Whether or not to parse the value for variable substitution. :return: a
 * variable object :rtype: pkg_config_tuple_t *
 */
pkg_config_tuple_t*
pkg_config_tuple_add (const pkg_config_client_t* client,
                      pkg_config_list_t* list,
                      const char* key,
                      const char* value,
                      bool parse)
{
  char* dequote_value;
  pkg_config_tuple_t* tuple = calloc (1, sizeof (pkg_config_tuple_t));

  pkg_config_tuple_find_delete (list, key);

  dequote_value = dequote (value);

  PKG_CONFIG_TRACE (client,
                    "adding tuple to @%p: %s => %s (parsed? %d)",
                    list,
                    key,
                    dequote_value,
                    parse);

  tuple->key = strdup (key);
  if (parse)
    tuple->value = pkg_config_tuple_parse (client, list, dequote_value);
  else
    tuple->value = strdup (dequote_value);

  pkg_config_list_insert (&tuple->iter, tuple, list);

  free (dequote_value);

  return tuple;
}

/*
 * !doc
 *
 * .. c:function:: char *pkg_config_tuple_find(const pkg_config_client_t
 * *client, pkg_config_list_t *list, const char *key)
 *
 *    Look up a variable in a variable list.
 *
 *    :param pkg_config_client_t* client: The pkg-config client object to
 * access. :param pkg_config_list_t* list: The variable list to search. :param
 * char* key: The variable name to search for. :return: the value of the
 * variable or ``NULL`` :rtype: char *
 */
char*
pkg_config_tuple_find (const pkg_config_client_t* client,
                       pkg_config_list_t* list,
                       const char* key)
{
  pkg_config_node_t* node;
  char* res;

  if ((res = pkg_config_tuple_find_global (client, key)) != NULL)
    return res;

  LIBPKG_CONFIG_FOREACH_LIST_ENTRY (list->head, node)
  {
    pkg_config_tuple_t* tuple = node->data;

    if (!strcmp (tuple->key, key))
      return tuple->value;
  }

  return NULL;
}

/*
 * !doc
 *
 * .. c:function:: char *pkg_config_tuple_parse(const pkg_config_client_t
 * *client, pkg_config_list_t *vars, const char *value)
 *
 *    Parse an expression for variable substitution.
 *
 *    :param pkg_config_client_t* client: The pkg-config client object to
 * access. :param pkg_config_list_t* list: The variable list to search for
 * variables (along side the global variable list). :param char* value: The
 * ``key=value`` string to parse. :return: the variable data with any
 * variables substituted :rtype: char *
 */
char*
pkg_config_tuple_parse (const pkg_config_client_t* client,
                        pkg_config_list_t* vars,
                        const char* value)
{
  /* Allocate the buffer dynamically to make sure it will at least fit the
     value provided it has no expansions. */
  char* buf;
  size_t bufn = strlen (value) + 1;

  if (bufn < PKG_CONFIG_BUFSIZE)
    bufn = PKG_CONFIG_BUFSIZE;

  buf = malloc (bufn);

  const char* ptr;
  char* bptr = buf;

  if (!(client->flags & LIBPKG_CONFIG_PKG_PKGF_FDO_SYSROOT_RULES))
  {
    if (*value == '/' && client->sysroot_dir != NULL &&
        strncmp (value, client->sysroot_dir, strlen (client->sysroot_dir)))
      bptr += pkg_config_strlcpy (buf, client->sysroot_dir, sizeof buf);
  }

  for (ptr = value; *ptr != '\0' && (size_t)(bptr - buf) < bufn  - 1; ptr++)
  {
    if (*ptr != '$' || (*ptr == '$' && *(ptr + 1) != '{'))
      *bptr++ = *ptr;
    else if (*(ptr + 1) == '{')
    {
      char varname[PKG_CONFIG_ITEM_SIZE];
      char* vend = varname + PKG_CONFIG_ITEM_SIZE - 1;
      char* vptr = varname;
      const char* pptr;
      char *kv, *parsekv;

      *vptr = '\0';

      for (pptr = ptr + 2; *pptr != '\0'; pptr++)
      {
        if (*pptr != '}')
        {
          if (vptr < vend)
            *vptr++ = *pptr;
          else
          {
            *vptr = '\0';
            /* Keep looking for '}'. */
          }
        }
        else
        {
          *vptr = '\0';
          break;
        }
      }

      ptr += (pptr - ptr);
      size_t n = bufn - (bptr - buf) - 1; /* Available. */

      kv = pkg_config_tuple_find_global (client, varname);
      if (kv != NULL)
      {
        size_t l = strlen (kv);
        strncpy (bptr, kv, n);
        bptr += l < n ? l : n;
      }
      else
      {
        kv = pkg_config_tuple_find (client, vars, varname);

        if (kv != NULL)
        {
          parsekv = pkg_config_tuple_parse (client, vars, kv);

          size_t l = strlen (parsekv);
          strncpy (bptr, parsekv, n);
          bptr += l < n ? l : n;

          free (parsekv);
        }
      }
    }
  }

  *bptr = '\0';

  /*
   * Sigh.  Somebody actually attempted to use freedesktop.org pkg-config's
   * broken sysroot support, which was written by somebody who did not
   * understand how sysroots are supposed to work.  This results in an
   * incorrect path being built as the sysroot will be prepended twice, once
   * explicitly, and once by variable expansion (the pkgconf approach).  We
   * could simply make ${pc_sysrootdir} blank, but sometimes it is necessary
   * to know the explicit sysroot path for other reasons, so we can't really
   * do that.
   *
   * As a result, we check to see if ${pc_sysrootdir} is prepended as a
   * duplicate, and if so, remove the prepend.  This allows us to handle both
   * our approach and the broken freedesktop.org implementation's approach.
   * Because a path can be shorter than ${pc_sysrootdir}, we do some checks
   * first to ensure it's safe to skip ahead in the string to scan for our
   * sysroot dir.
   *
   * Finally, we call pkg_config_path_relocate() to clean the path of spurious
   * elements.
   */
  char* result;
  if (*buf == '/' && client->sysroot_dir != NULL &&
      strcmp (client->sysroot_dir, "/") != 0 &&
      strlen (buf) > strlen (client->sysroot_dir) &&
      strstr (buf + strlen (client->sysroot_dir), client->sysroot_dir) !=
          NULL)
  {
    char cleanpath[PKG_CONFIG_ITEM_SIZE];

    pkg_config_strlcpy (
        cleanpath, buf + strlen (client->sysroot_dir), sizeof cleanpath);
    pkg_config_path_relocate (cleanpath, sizeof cleanpath);

    result = strdup (cleanpath);
  }
  else
    result = strdup (buf);

  free (buf);
  return result;
}

/*
 * !doc
 *
 * .. c:function:: void pkg_config_tuple_free_entry(pkg_config_tuple_t *tuple,
 * pkg_config_list_t *list)
 *
 *    Deletes a variable object, removing it from any variable lists and
 * releasing any memory associated with it.
 *
 *    :param pkg_config_tuple_t* tuple: The variable object to release.
 *    :param pkg_config_list_t* list: The variable list the variable object is
 * attached to. :return: nothing
 */
void
pkg_config_tuple_free_entry (pkg_config_tuple_t* tuple,
                             pkg_config_list_t* list)
{
  pkg_config_list_delete (&tuple->iter, list);

  free (tuple->key);
  free (tuple->value);
  free (tuple);
}

/*
 * !doc
 *
 * .. c:function:: void pkg_config_tuple_free(pkg_config_list_t *list)
 *
 *    Deletes a variable list and any variables attached to it.
 *
 *    :param pkg_config_list_t* list: The variable list to delete.
 *    :return: nothing
 */
void
pkg_config_tuple_free (pkg_config_list_t* list)
{
  pkg_config_node_t *node, *next;

  LIBPKG_CONFIG_FOREACH_LIST_ENTRY_SAFE (list->head, next, node)
  pkg_config_tuple_free_entry (node->data, list);

  pkg_config_list_zero (list);
}
