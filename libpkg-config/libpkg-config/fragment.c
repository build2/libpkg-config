/*
 * fragment.c
 * Management of fragment lists.
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
 * libpkg-config `fragment` module
 * ===============================
 *
 * The `fragment` module provides low-level management and rendering of
 * fragment lists.  A `fragment list` contains various `fragments` of text
 * (such as ``-I /usr/include``) in a matter which is composable, mergeable
 * and reorderable.
 */

struct pkg_config_fragment_check
{
  char* token;
  size_t len;
};

static const struct pkg_config_fragment_check check_fragments[] = {
  {"-framework", 10},
  {"-isystem", 8},
  {"-idirafter", 10},
  {"-pthread", 8},
  {"-Wa,", 4},
  {"-Wl,", 4},
  {"-Wp,", 4},
  {"-trigraphs", 10},
  {"-pedantic", 9},
  {"-ansi", 5},
  {"-std=", 5},
  {"-stdlib=", 8},
  {"-include", 8},
  {"-nostdinc", 9},
  {"-nostdlibinc", 12},
  {"-nobuiltininc", 13}};

static inline bool
pkg_config_fragment_is_unmergeable (const char* string)
{
  if (*string != '-')
    return true;

  size_t i;
  for (i = 0; i < PKG_CONFIG_ARRAY_SIZE (check_fragments); i++)
    if (!strncmp (string, check_fragments[i].token, check_fragments[i].len))
      return true;

  /* only one pair of {-flag, arg} may be merged together */
  if (strchr (string, ' ') != NULL)
    return false;

  return false;
}

static inline bool
pkg_config_fragment_should_munge (const char* string, const char* sysroot_dir)
{
  if (*string != '/')
    return false;

  if (sysroot_dir != NULL &&
      strncmp (sysroot_dir, string, strlen (sysroot_dir)))
    return true;

  return false;
}

static inline bool
pkg_config_fragment_is_special (const char* string)
{
  if (*string != '-')
    return true;

  if (!strncmp (string, "-lib:", 5))
    return true;

  return pkg_config_fragment_is_unmergeable (string);
}

static inline void
pkg_config_fragment_munge (const pkg_config_client_t* client,
                           char* buf,
                           size_t buflen,
                           const char* source,
                           const char* sysroot_dir)
{
  *buf = '\0';

  if (sysroot_dir == NULL)
    sysroot_dir = pkg_config_tuple_find_global (client, "pc_sysrootdir");

  if (sysroot_dir != NULL &&
      pkg_config_fragment_should_munge (source, sysroot_dir))
    pkg_config_strlcat (buf, sysroot_dir, buflen);

  pkg_config_strlcat (buf, source, buflen);

  if (*buf == '/' &&
      !(client->flags & LIBPKG_CONFIG_PKG_PKGF_DONT_RELOCATE_PATHS))
    pkg_config_path_relocate (buf, buflen);
}

static inline char*
pkg_config_fragment_copy_munged (const pkg_config_client_t* client,
                                 const char* source)
{
  char mungebuf[PKG_CONFIG_ITEM_SIZE];
  pkg_config_fragment_munge (
      client, mungebuf, sizeof mungebuf, source, client->sysroot_dir);
  return strdup (mungebuf);
}

/*
 * !doc
 *
 * .. c:function:: void pkg_config_fragment_add(const pkg_config_client_t
 * *client, pkg_config_list_t *list, const char *string)
 *
 *    Adds a `fragment` of text to a `fragment list`, possibly modifying the
 * fragment if a sysroot is set.
 *
 *    :param pkg_config_client_t* client: The pkg-config client being
 * accessed. :param pkg_config_list_t* list: The fragment list. :param char*
 * string: The string of text to add as a fragment to the fragment list.
 * :return: nothing
 */
void
pkg_config_fragment_add (const pkg_config_client_t* client,
                         pkg_config_list_t* list,
                         const char* string)
{
  pkg_config_fragment_t* frag;

  if (*string == '\0')
    return;

  if (strlen (string) > 1 && !pkg_config_fragment_is_special (string))
  {
    /* If the previous fragment is of the -I, -L, -F, or -l type and has no
     * path specified, then consider the current fragment as a separately
     * specified path and don't split it into the type and data.
     */
    char type;
    const char* data;

    pkg_config_fragment_t* p = list->tail != NULL ? list->tail->data : NULL;
    char t = p != NULL ? p->type : '\0';

    if (p != NULL && (t == 'I' || t == 'L' || t == 'F' || t == 'l') &&
        p->data[0] == '\0')
    {
      type = '\0';
      data = string;
    }
    else
    {
      type = *(string + 1);
      data = string + 2;
    }

    frag = calloc (1, sizeof (pkg_config_fragment_t));

    frag->type = type;
    frag->data = pkg_config_fragment_copy_munged (client, data);

    PKG_CONFIG_TRACE (client,
                      "added fragment {%c, '%s'} to list @%p",
                      frag->type,
                      frag->data,
                      list);
  }
  else
  {
    char mungebuf[PKG_CONFIG_ITEM_SIZE];

    if (list->tail != NULL && list->tail->data != NULL &&
        (client->flags & LIBPKG_CONFIG_PKG_PKGF_MERGE_SPECIAL_FRAGMENTS) != 0)
    {
      pkg_config_fragment_t* parent = list->tail->data;

      /* only attempt to merge 'special' fragments together */
      if (!parent->type && pkg_config_fragment_is_unmergeable (parent->data))
      {
        size_t len;
        char* newdata;

        pkg_config_fragment_munge (
            client, mungebuf, sizeof mungebuf, string, NULL);

        len = strlen (parent->data) + strlen (mungebuf) + 2;
        newdata = malloc (len);

        pkg_config_strlcpy (newdata, parent->data, len);
        pkg_config_strlcat (newdata, " ", len);
        pkg_config_strlcat (newdata, mungebuf, len);

        PKG_CONFIG_TRACE (
            client,
            "merging '%s' to '%s' to form fragment {'%s'} in list @%p",
            mungebuf,
            parent->data,
            newdata,
            list);

        free (parent->data);
        parent->data = newdata;
        parent->merged = true;

        /* use a copy operation to force a dedup */
        pkg_config_list_delete (&parent->iter, list);
        pkg_config_fragment_copy (client, list, parent, false);

        /* the fragment list now (maybe) has the copied node, so free the
         * original */
        free (parent->data);
        free (parent);

        return;
      }
    }

    frag = calloc (1, sizeof (pkg_config_fragment_t));

    frag->type = 0;
    frag->data = strdup (string);

    PKG_CONFIG_TRACE (client,
                      "created special fragment {'%s'} in list @%p",
                      frag->data,
                      list);
  }

  pkg_config_list_insert_tail (&frag->iter, frag, list);
}

static inline pkg_config_fragment_t*
pkg_config_fragment_lookup (pkg_config_list_t* list,
                            const pkg_config_fragment_t* base)
{
  pkg_config_node_t* node;

  LIBPKG_CONFIG_FOREACH_LIST_ENTRY_REVERSE (list->tail, node)
  {
    pkg_config_fragment_t* frag = node->data;

    if (base->type != frag->type)
      continue;

    if (!strcmp (base->data, frag->data))
      return frag;
  }

  return NULL;
}

static inline bool
pkg_config_fragment_can_merge_back (const pkg_config_fragment_t* base,
                                    unsigned int flags,
                                    bool is_private)
{
  (void)flags;

  if (base->type == 'l')
  {
    if (is_private)
      return false;

    return true;
  }

  if (base->type == 'F')
    return false;
  if (base->type == 'L')
    return false;
  if (base->type == 'I')
    return false;

  return true;
}

static inline bool
pkg_config_fragment_can_merge (const pkg_config_fragment_t* base,
                               unsigned int flags,
                               bool is_private)
{
  (void)flags;

  if (is_private)
    return false;

  return pkg_config_fragment_is_unmergeable (base->data);
}

static inline pkg_config_fragment_t*
pkg_config_fragment_exists (pkg_config_list_t* list,
                            const pkg_config_fragment_t* base,
                            unsigned int flags,
                            bool is_private)
{
  if (!pkg_config_fragment_can_merge_back (base, flags, is_private))
    return NULL;

  if (!pkg_config_fragment_can_merge (base, flags, is_private))
    return NULL;

  return pkg_config_fragment_lookup (list, base);
}

static inline bool
pkg_config_fragment_should_merge (const pkg_config_fragment_t* base)
{
  const pkg_config_fragment_t* parent;

  /* if we are the first fragment, that means the next fragment is the same,
   * so it's always safe. */
  if (base->iter.prev == NULL)
    return true;

  /* this really shouldn't ever happen, but handle it */
  parent = base->iter.prev->data;
  if (parent == NULL)
    return true;

  switch (parent->type)
  {
  case 'l':
  case 'L':
  case 'I':
    return true;
  default:
    return !base->type || parent->type == base->type;
  }
}

/*
 * !doc
 *
 * .. c:function:: bool pkg_config_fragment_has_system_dir(const
 * pkg_config_client_t *client, const pkg_config_fragment_t *frag)
 *
 *    Checks if a `fragment` contains a `system path`.  System paths are
 * detected at compile time and optionally overridden by the
 * ``PKG_CONFIG_SYSTEM_INCLUDE_PATH`` and ``PKG_CONFIG_SYSTEM_LIBRARY_PATH``
 * environment variables.
 *
 *    :param pkg_config_client_t* client: The pkg-config client object the
 * fragment belongs to. :param pkg_config_fragment_t* frag: The fragment being
 * checked. :return: true if the fragment contains a system path, else false
 *    :rtype: bool
 */
bool
pkg_config_fragment_has_system_dir (const pkg_config_client_t* client,
                                    const pkg_config_fragment_t* frag)
{
  const pkg_config_list_t* check_paths = NULL;

  switch (frag->type)
  {
  case 'L':
    check_paths = &client->filter_libdirs;
    break;
  case 'I':
    check_paths = &client->filter_includedirs;
    break;
  default:
    return false;
  }

  return pkg_config_path_match_list (frag->data, check_paths);
}

/*
 * !doc
 *
 * .. c:function:: void pkg_config_fragment_copy(const pkg_config_client_t
 * *client, pkg_config_list_t *list, const pkg_config_fragment_t *base, bool
 * is_private)
 *
 *    Copies a `fragment` to another `fragment list`, possibly removing a
 * previous copy of the `fragment` in a process known as `mergeback`.
 *
 *    :param pkg_config_client_t* client: The pkg-config client being
 * accessed. :param pkg_config_list_t* list: The list the fragment is being
 * added to. :param pkg_config_fragment_t* base: The fragment being copied.
 *    :param bool is_private: Whether the fragment list is a `private`
 * fragment list (static linking). :return: nothing
 */
void
pkg_config_fragment_copy (const pkg_config_client_t* client,
                          pkg_config_list_t* list,
                          const pkg_config_fragment_t* base,
                          bool is_private)
{
  pkg_config_fragment_t* frag;

  if ((client->flags & LIBPKG_CONFIG_PKG_PKGF_MERGE_SPECIAL_FRAGMENTS) != 0)
  {
    if ((frag = pkg_config_fragment_exists (
             list, base, client->flags, is_private)) != NULL)
    {
      if (pkg_config_fragment_should_merge (frag))
        pkg_config_fragment_delete (list, frag);
    }
    else if (!is_private &&
             !pkg_config_fragment_can_merge_back (
                 base, client->flags, is_private) &&
             (pkg_config_fragment_lookup (list, base) != NULL))
      return;
  }

  frag = calloc (1, sizeof (pkg_config_fragment_t));

  frag->type = base->type;
  frag->merged = base->merged;
  if (base->data != NULL)
    frag->data = strdup (base->data);

  pkg_config_list_insert_tail (&frag->iter, frag, list);
}

/*
 * !doc
 *
 * .. c:function:: void pkg_config_fragment_copy_list(const
 * pkg_config_client_t *client, pkg_config_list_t *list, const
 * pkg_config_list_t *base)
 *
 *    Copies a `fragment list` to another `fragment list`, possibly removing a
 * previous copy of the fragments in a process known as `mergeback`.
 *
 *    :param pkg_config_client_t* client: The pkg-config client being
 * accessed. :param pkg_config_list_t* list: The list the fragments are being
 * added to. :param pkg_config_list_t* base: The list the fragments are being
 * copied from. :return: nothing
 */
void
pkg_config_fragment_copy_list (const pkg_config_client_t* client,
                               pkg_config_list_t* list,
                               const pkg_config_list_t* base)
{
  pkg_config_node_t* node;

  LIBPKG_CONFIG_FOREACH_LIST_ENTRY (base->head, node)
  {
    pkg_config_fragment_t* frag = node->data;

    pkg_config_fragment_copy (client, list, frag, true);
  }
}

/*
 * !doc
 *
 * .. c:function:: void pkg_config_fragment_filter(const pkg_config_client_t
 * *client, pkg_config_list_t *dest, pkg_config_list_t *src,
 * pkg_config_fragment_filter_func_t filter_func)
 *
 *    Copies a `fragment list` to another `fragment list` which match a
 * user-specified filtering function.
 *
 *    :param pkg_config_client_t* client: The pkg-config client being
 * accessed. :param pkg_config_list_t* dest: The destination list. :param
 * pkg_config_list_t* src: The source list. :param
 * pkg_config_fragment_filter_func_t filter_func: The filter function to use.
 * :param void* data: Optional data to pass to the filter function. :return:
 * nothing
 */
void
pkg_config_fragment_filter (const pkg_config_client_t* client,
                            pkg_config_list_t* dest,
                            pkg_config_list_t* src,
                            pkg_config_fragment_filter_func_t filter_func,
                            void* data)
{
  pkg_config_node_t* node;

  LIBPKG_CONFIG_FOREACH_LIST_ENTRY (src->head, node)
  {
    pkg_config_fragment_t* frag = node->data;

    if (filter_func (client, frag, data))
      pkg_config_fragment_copy (client, dest, frag, true);
  }
}

static inline char*
fragment_quote (const pkg_config_fragment_t* frag)
{
  const char* src = frag->data;
  ssize_t outlen = strlen (src) + 10;
  char *out, *dst;

  if (frag->data == NULL)
    return NULL;

  out = dst = calloc (outlen, 1);

  for (; *src; src++)
  {
    if (((*src < ' ') ||
         (*src >= (' ' + (frag->merged ? 1 : 0)) && *src < '$') ||
         (*src > '$' && *src < '(') || (*src > ')' && *src < '+') ||
         (*src > ':' && *src < '=') || (*src > '=' && *src < '@') ||
         (*src > 'Z' && *src < '^') || (*src == '`') ||
         (*src > 'z' && *src < '~') || (*src > '~')))
      *dst++ = '\\';

    *dst++ = *src;

    if ((ptrdiff_t) (dst - out) + 2 > outlen)
    {
      ptrdiff_t offset = dst - out;
      outlen *= 2;
      out = realloc (out, outlen);
      dst = out + offset;
    }
  }

  *dst = 0;
  return out;
}

static inline size_t
pkg_config_fragment_len (const pkg_config_fragment_t* frag)
{
  size_t len = 1;

  if (frag->type)
    len += 2;

  if (frag->data != NULL)
  {
    char* quoted = fragment_quote (frag);
    len += strlen (quoted);
    free (quoted);
  }

  return len;
}

static size_t
fragment_render_len (const pkg_config_list_t* list, bool escape)
{
  (void)escape;

  size_t out = 1; /* trailing nul */
  pkg_config_node_t* node;

  LIBPKG_CONFIG_FOREACH_LIST_ENTRY (list->head, node)
  {
    const pkg_config_fragment_t* frag = node->data;
    out += pkg_config_fragment_len (frag);
  }

  return out;
}

static void
fragment_render_buf (const pkg_config_list_t* list,
                     char* buf,
                     size_t buflen,
                     bool escape)
{
  (void)escape;

  pkg_config_node_t* node;
  char* bptr = buf;

  memset (buf, 0, buflen);

  LIBPKG_CONFIG_FOREACH_LIST_ENTRY (list->head, node)
  {
    const pkg_config_fragment_t* frag = node->data;
    size_t buf_remaining = buflen - (bptr - buf);
    char* quoted = fragment_quote (frag);

    if (strlen (quoted) > buf_remaining)
    {
      free (quoted);
      break;
    }

    if (frag->type)
    {
      *bptr++ = '-';
      *bptr++ = frag->type;
    }

    if (quoted != NULL)
    {
      bptr += pkg_config_strlcpy (bptr, quoted, buf_remaining);
      free (quoted);
    }

    *bptr++ = ' ';
  }

  *bptr = '\0';
}

static const pkg_config_fragment_render_ops_t default_render_ops = {
    .render_len = fragment_render_len, .render_buf = fragment_render_buf};

/*
 * !doc
 *
 * .. c:function:: size_t pkg_config_fragment_render_len(const
 * pkg_config_list_t *list, bool escape, const
 * pkg_config_fragment_render_ops_t *ops)
 *
 *    Calculates the required memory to store a `fragment list` when rendered
 * as a string.
 *
 *    :param pkg_config_list_t* list: The `fragment list` being rendered.
 *    :param bool escape: Whether or not to escape special shell characters
 * (deprecated). :param pkg_config_fragment_render_ops_t* ops: An optional ops
 * structure to use for custom renderers, else ``NULL``. :return: the amount
 * of bytes required to represent the `fragment list` when rendered :rtype:
 * size_t
 */
size_t
pkg_config_fragment_render_len (const pkg_config_list_t* list,
                                bool escape,
                                const pkg_config_fragment_render_ops_t* ops)
{
  (void)escape;

  ops = ops != NULL ? ops : &default_render_ops;
  return ops->render_len (list, true);
}

/*
 * !doc
 *
 * .. c:function:: void pkg_config_fragment_render_buf(const pkg_config_list_t
 * *list, char *buf, size_t buflen, bool escape, const
 * pkg_config_fragment_render_ops_t *ops)
 *
 *    Renders a `fragment list` into a buffer.
 *
 *    :param pkg_config_list_t* list: The `fragment list` being rendered.
 *    :param char* buf: The buffer to render the fragment list into.
 *    :param size_t buflen: The length of the buffer.
 *    :param bool escape: Whether or not to escape special shell characters
 * (deprecated). :param pkg_config_fragment_render_ops_t* ops: An optional ops
 * structure to use for custom renderers, else ``NULL``. :return: nothing
 */
void
pkg_config_fragment_render_buf (const pkg_config_list_t* list,
                                char* buf,
                                size_t buflen,
                                bool escape,
                                const pkg_config_fragment_render_ops_t* ops)
{
  (void)escape;

  ops = ops != NULL ? ops : &default_render_ops;
  ops->render_buf (list, buf, buflen, true);
}

/*
 * !doc
 *
 * .. c:function:: char *pkg_config_fragment_render(const pkg_config_list_t
 * *list)
 *
 *    Allocate memory and render a `fragment list` into it.
 *
 *    :param pkg_config_list_t* list: The `fragment list` being rendered.
 *    :param bool escape: Whether or not to escape special shell characters
 * (deprecated). :param pkg_config_fragment_render_ops_t* ops: An optional ops
 * structure to use for custom renderers, else ``NULL``. :return: An allocated
 * string containing the rendered `fragment list`. :rtype: char *
 */
char*
pkg_config_fragment_render (const pkg_config_list_t* list,
                            bool escape,
                            const pkg_config_fragment_render_ops_t* ops)
{
  (void)escape;

  size_t buflen = pkg_config_fragment_render_len (list, true, ops);
  char* buf = calloc (1, buflen);

  pkg_config_fragment_render_buf (list, buf, buflen, true, ops);

  return buf;
}

/*
 * !doc
 *
 * .. c:function:: void pkg_config_fragment_delete(pkg_config_list_t *list,
 * pkg_config_fragment_t *node)
 *
 *    Delete a `fragment node` from a `fragment list`.
 *
 *    :param pkg_config_list_t* list: The `fragment list` to delete from.
 *    :param pkg_config_fragment_t* node: The `fragment node` to delete.
 *    :return: nothing
 */
void
pkg_config_fragment_delete (pkg_config_list_t* list,
                            pkg_config_fragment_t* node)
{
  pkg_config_list_delete (&node->iter, list);

  free (node->data);
  free (node);
}

/*
 * !doc
 *
 * .. c:function:: void pkg_config_fragment_free(pkg_config_list_t *list)
 *
 *    Delete an entire `fragment list`.
 *
 *    :param pkg_config_list_t* list: The `fragment list` to delete.
 *    :return: nothing
 */
void
pkg_config_fragment_free (pkg_config_list_t* list)
{
  pkg_config_node_t *node, *next;

  LIBPKG_CONFIG_FOREACH_LIST_ENTRY_SAFE (list->head, next, node)
  {
    pkg_config_fragment_t* frag = node->data;

    free (frag->data);
    free (frag);
  }
}

/*
 * !doc
 *
 * .. c:function:: bool pkg_config_fragment_parse(const pkg_config_client_t
 * *client, pkg_config_list_t *list, pkg_config_list_t *vars, const char
 * *value)
 *
 *    Parse a string into a `fragment list`.
 *
 *    :param pkg_config_client_t* client: The pkg-config client being
 * accessed. :param pkg_config_list_t* list: The `fragment list` to add the
 * fragment entries to. :param pkg_config_list_t* vars: A list of variables to
 * use for variable substitution. :param char* value: The string to parse into
 * fragments. :return: true on success, false on parse error
 */
bool
pkg_config_fragment_parse (const pkg_config_client_t* client,
                           pkg_config_list_t* list,
                           pkg_config_list_t* vars,
                           const char* value)
{
  int i, ret, argc;
  char** argv;
  char* repstr = pkg_config_tuple_parse (client, vars, value);

  PKG_CONFIG_TRACE (client, "post-subst: [%s] -> [%s]", value, repstr);

  ret = pkg_config_argv_split (repstr, &argc, &argv);
  if (ret < 0)
  {
    PKG_CONFIG_TRACE (client, "unable to parse fragment string [%s]", repstr);
    free (repstr);
    return false;
  }

  for (i = 0; i < argc; i++)
  {
    if (argv[i] == NULL)
    {
      PKG_CONFIG_TRACE (client,
                        "parsed fragment string is inconsistent: argc = %d "
                        "while argv[%d] == NULL",
                        argc,
                        i);
      pkg_config_argv_free (argv);
      free (repstr);
      return false;
    }

    pkg_config_fragment_add (client, list, argv[i]);
  }

  pkg_config_argv_free (argv);
  free (repstr);

  return true;
}
