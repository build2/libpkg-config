/*
 * pkg.c
 * higher-level dependency graph compilation, management and manipulation
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
 * libpkg-config `pkg` module
 * ==========================
 *
 * The `pkg` module provides dependency resolution services and the overall
 * `.pc` file parsing routines.
 */

static inline bool
str_has_suffix (const char* str, const char* suffix)
{
  size_t str_len = strlen (str);
  size_t suf_len = strlen (suffix);

  if (str_len < suf_len)
    return false;

  return !strncasecmp (str + str_len - suf_len, suffix, suf_len);
}

static char*
pkg_get_parent_dir (pkg_config_pkg_t* pkg)
{
  char buf[PKG_CONFIG_ITEM_SIZE], *pathbuf;

  pkg_config_strlcpy (buf, pkg->filename, sizeof buf);
  pathbuf = strrchr (buf, LIBPKG_CONFIG_DIR_SEP_S);
  if (pathbuf == NULL)
    pathbuf = strrchr (buf, '/');
  if (pathbuf != NULL)
    pathbuf[0] = '\0';

  return strdup (buf);
}

typedef unsigned int /* eflags */ (*pkg_config_pkg_parser_keyword_func_t) (
    const pkg_config_client_t* client,
    pkg_config_pkg_t* pkg,
    const char* keyword,
    const size_t lineno,
    const ptrdiff_t offset,
    const char* value);
typedef struct
{
  const char* keyword;
  const pkg_config_pkg_parser_keyword_func_t func;
  const ptrdiff_t offset;
} pkg_config_pkg_parser_keyword_pair_t;

static int
pkg_config_pkg_parser_keyword_pair_cmp (const void* key, const void* ptr)
{
  const pkg_config_pkg_parser_keyword_pair_t* pair = ptr;
  return strcasecmp (key, pair->keyword);
}

static unsigned int
pkg_config_pkg_parser_tuple_func (const pkg_config_client_t* client,
                                  pkg_config_pkg_t* pkg,
                                  const char* keyword,
                                  const size_t lineno,
                                  const ptrdiff_t offset,
                                  const char* value)
{
  (void)keyword;
  (void)lineno;

  char** dest = (char**)((char*)pkg + offset);
  *dest = pkg_config_tuple_parse (client, &pkg->vars, value);
  return LIBPKG_CONFIG_ERRF_OK;
}

static unsigned int
pkg_config_pkg_parser_version_func (const pkg_config_client_t* client,
                                    pkg_config_pkg_t* pkg,
                                    const char* keyword,
                                    const size_t lineno,
                                    const ptrdiff_t offset,
                                    const char* value)
{
  (void)keyword;
  (void)lineno;
  char *p, *i;
  size_t len;
  char** dest = (char**)((char*)pkg + offset);

  /* cut at any detected whitespace */
  p = pkg_config_tuple_parse (client, &pkg->vars, value);

  len = strcspn (p, " \t");
  if (len != strlen (p))
  {
    i = p + (ptrdiff_t)len;
    *i = '\0';

    /* While this should probably be an error, it is a bit dodgy to change it
       now since things that worked before might stop working. */
    pkg_config_warn (
      client,
      pkg->filename,
      lineno,
      "version field with whitespaces trimmed to '%s'",
      p);
  }

  *dest = p;
  return LIBPKG_CONFIG_ERRF_OK;
}

static unsigned int
pkg_config_pkg_parser_fragment_func (const pkg_config_client_t* client,
                                     pkg_config_pkg_t* pkg,
                                     const char* keyword,
                                     const size_t lineno,
                                     const ptrdiff_t offset,
                                     const char* value)
{
  pkg_config_list_t* dest = (pkg_config_list_t*)((char*)pkg + offset);
  if (pkg_config_fragment_parse (client, dest, &pkg->vars, value))
    return LIBPKG_CONFIG_ERRF_OK;

  unsigned int eflags = LIBPKG_CONFIG_ERRF_FILE_INVALID_SYNTAX;

  pkg_config_error (client,
                    eflags,
                    pkg->filename,
                    lineno,
                    "unable to parse field '%s' value '%s' into arguments",
                    keyword,
                    value);
  return eflags;
}

static unsigned int
pkg_config_pkg_parser_dependency_func (const pkg_config_client_t* client,
                                       pkg_config_pkg_t* pkg,
                                       const char* keyword,
                                       const size_t lineno,
                                       const ptrdiff_t offset,
                                       const char* value)
{
  (void)keyword;
  (void)lineno;

  pkg_config_list_t* dest = (pkg_config_list_t*)((char*)pkg + offset);
  pkg_config_dependency_parse (client, pkg, dest, value, 0);
  return LIBPKG_CONFIG_ERRF_OK;
}

/* a variant of pkg_config_pkg_parser_dependency_func which colors the
 * dependency node as an "internal" dependency. */
static unsigned int
pkg_config_pkg_parser_internal_dependency_func (
    const pkg_config_client_t* client,
    pkg_config_pkg_t* pkg,
    const char* keyword,
    const size_t lineno,
    const ptrdiff_t offset,
    const char* value)
{
  (void)keyword;
  (void)lineno;

  pkg_config_list_t* dest = (pkg_config_list_t*)((char*)pkg + offset);
  pkg_config_dependency_parse (
      client, pkg, dest, value, LIBPKG_CONFIG_PKG_DEPF_INTERNAL);
  return LIBPKG_CONFIG_ERRF_OK;
}

/* keep this in alphabetical order */
static const pkg_config_pkg_parser_keyword_pair_t
pkg_config_pkg_parser_keyword_funcs[] = {
  {"CFLAGS",
   pkg_config_pkg_parser_fragment_func,
   offsetof (pkg_config_pkg_t, cflags)},
  {"CFLAGS.private",
   pkg_config_pkg_parser_fragment_func,
   offsetof (pkg_config_pkg_t, cflags_private)},
  {"Conflicts",
   pkg_config_pkg_parser_dependency_func,
   offsetof (pkg_config_pkg_t, conflicts)},
  {"Description",
   pkg_config_pkg_parser_tuple_func,
   offsetof (pkg_config_pkg_t, description)},
  {"LIBS",
   pkg_config_pkg_parser_fragment_func,
   offsetof (pkg_config_pkg_t, libs)},
  {"LIBS.private",
   pkg_config_pkg_parser_fragment_func,
   offsetof (pkg_config_pkg_t, libs_private)},
  {"Name",
   pkg_config_pkg_parser_tuple_func,
   offsetof (pkg_config_pkg_t, realname)},
  {"Requires",
   pkg_config_pkg_parser_dependency_func,
   offsetof (pkg_config_pkg_t, required)},
  {"Requires.internal",
   pkg_config_pkg_parser_internal_dependency_func,
   offsetof (pkg_config_pkg_t, requires_private)},
  {"Requires.private",
   pkg_config_pkg_parser_dependency_func,
   offsetof (pkg_config_pkg_t, requires_private)},
  {"Version",
   pkg_config_pkg_parser_version_func,
   offsetof (pkg_config_pkg_t, version)}
};

static unsigned int /* eflags */
pkg_config_pkg_parser_keyword_set (void* opaque,
                                   const size_t lineno,
                                   const char* keyword,
                                   const char* value)
{
  pkg_config_pkg_t* pkg = opaque;

  const pkg_config_pkg_parser_keyword_pair_t* pair =
      bsearch (keyword,
               pkg_config_pkg_parser_keyword_funcs,
               PKG_CONFIG_ARRAY_SIZE (pkg_config_pkg_parser_keyword_funcs),
               sizeof (pkg_config_pkg_parser_keyword_pair_t),
               pkg_config_pkg_parser_keyword_pair_cmp);

  /* @@ Is this really ok or should it be an error? */
  if (pair == NULL || pair->func == NULL)
    return LIBPKG_CONFIG_ERRF_OK;

  return pair->func (pkg->owner, pkg, keyword, lineno, pair->offset, value);
}

static const char*
determine_prefix (const pkg_config_pkg_t* pkg, char* buf, size_t buflen)
{
  char* pathiter;

  pkg_config_strlcpy (buf, pkg->filename, buflen);
  pkg_config_path_relocate (buf, buflen);

  pathiter = strrchr (buf, LIBPKG_CONFIG_DIR_SEP_S);
  if (pathiter == NULL)
    pathiter = strrchr (buf, '/');
  if (pathiter != NULL)
    pathiter[0] = '\0';

  pathiter = strrchr (buf, LIBPKG_CONFIG_DIR_SEP_S);
  if (pathiter == NULL)
    pathiter = strrchr (buf, '/');
  if (pathiter == NULL)
    return NULL;

  /* parent dir is not pkgconfig, can't relocate then */
  if (strcmp (pathiter + 1, "pkgconfig"))
    return NULL;

  /* okay, work backwards and do it again. */
  pathiter[0] = '\0';
  pathiter = strrchr (buf, LIBPKG_CONFIG_DIR_SEP_S);
  if (pathiter == NULL)
    pathiter = strrchr (buf, '/');
  if (pathiter == NULL)
    return NULL;

  pathiter[0] = '\0';

  return buf;
}

/* Convert a path to pkg-config value. Return NULL on the memory allocation
 * failure.
 *
 * Specifically, escape backslashes, spaces, and quotes with a backslash.
 *
 * This is useful for things like prefix/pcfiledir which might get injected
 * at runtime and are not sourced from the .pc file. For example:
 *
 * C:\foo bar\baz  ->  C:\\foo\ bar\\baz
 * /foo bar/baz    ->  /foo\ bar/baz
 */
static char*
convert_path_to_value (const char* path)
{
  const char cs[] = " \\\"'";

  /* Calculate the number of characters which needs to be escaped.
   */
  size_t n = 0;
  const char* p = path;

  for (;;)
  {
    p = strpbrk (p, cs);

    if (p != NULL)
    {
      ++n;
      ++p;
    }
    else
      break;
  }

  /* Let's optimize for the common case (on POSIX), when no characters need
   * to be escaped.
   */
  if (n == 0)
    return strdup (path);

  char* buf = malloc (strlen (path) + n + 1);

  if (buf != NULL)
  {
    char* bp = buf;
    const char* p = path;

    for (;;)
    {
      const char* sp = strpbrk (p, cs);

      if (sp != NULL)
      {
        size_t n = sp - p;

        if (n != 0)
        {
          strncpy (bp, p, n);
          bp += n;
        }

        *bp++ = '\\';
        *bp++ = *sp;
        p = sp + 1;
      }
      else
      {
        /* Copy the trailing part including the \0 character and bail out.
         */
        strcpy (bp, p);
        break;
      }
    }
  }

  return buf;
}

static void
remove_additional_separators (char* buf)
{
  char* p = buf;

  while (*p)
  {
    if (*p == '/')
    {
      char* q;

      q = ++p;
      while (*q && *q == '/') q++;

      if (p != q)
        memmove (p, q, strlen (q) + 1);
    }
    else
    {
      p++;
    }
  }
}

static void
canonicalize_path (char* buf)
{
  remove_additional_separators (buf);
}

static bool
is_path_prefix_equal (const char* path1, const char* path2, size_t path2_len)
{
#ifdef _WIN32
  return !_strnicmp (path1, path2, path2_len);
#else
  return !strncmp (path1, path2, path2_len);
#endif
}

static unsigned int /* eflags */
pkg_config_pkg_parser_value_set (void* opaque,
                                 const size_t lineno,
                                 const char* keyword,
                                 const char* value)
{
  pkg_config_pkg_t* pkg = opaque;

  (void)lineno;

  if (!(pkg->owner->flags & LIBPKG_CONFIG_PKG_PKGF_REDEFINE_PREFIX))
  {
    pkg_config_tuple_add (pkg->owner, &pkg->vars, keyword, value, true);
    return LIBPKG_CONFIG_ERRF_OK;
  }

  char canonicalized_value[PKG_CONFIG_ITEM_SIZE];
  pkg_config_strlcpy (canonicalized_value, value, sizeof canonicalized_value);
  canonicalize_path (canonicalized_value);

  /* Some pc files will use absolute paths for all of their directories
   * which is broken when redefining the prefix. We try to outsmart the
   * file and rewrite any directory that starts with the same prefix.
   */
  if (pkg->orig_prefix &&
      is_path_prefix_equal (canonicalized_value,
                            pkg->orig_prefix->value,
                            strlen (pkg->orig_prefix->value)))
  {
    char newvalue[PKG_CONFIG_ITEM_SIZE];

    pkg_config_strlcpy (newvalue, pkg->prefix->value, sizeof newvalue);
    pkg_config_strlcat (newvalue,
                        canonicalized_value +
                            strlen (pkg->orig_prefix->value),
                        sizeof newvalue);
    pkg_config_tuple_add (pkg->owner, &pkg->vars, keyword, newvalue, false);
  }
  else if (strcmp (keyword, pkg->owner->prefix_varname))
    pkg_config_tuple_add (pkg->owner, &pkg->vars, keyword, value, true);
  else
  {
    char pathbuf[PKG_CONFIG_ITEM_SIZE];
    const char* relvalue = determine_prefix (pkg, pathbuf, sizeof pathbuf);

    if (relvalue != NULL)
    {
      char* prefix_value = convert_path_to_value (relvalue);
      if (prefix_value == NULL)
        return LIBPKG_CONFIG_ERRF_MEMORY;

      pkg->orig_prefix = pkg_config_tuple_add (
          pkg->owner, &pkg->vars, "orig_prefix", canonicalized_value, true);
      pkg->prefix = pkg_config_tuple_add (
          pkg->owner, &pkg->vars, keyword, prefix_value, false);
      free (prefix_value);
    }
    else
      pkg_config_tuple_add (pkg->owner, &pkg->vars, keyword, value, true);
  }

  return LIBPKG_CONFIG_ERRF_OK;
}

typedef struct
{
  const char* field;
  const ptrdiff_t offset;
} pkg_config_pkg_validity_check_t;

static const pkg_config_pkg_validity_check_t pkg_config_pkg_validations[] = {
  {"Name", offsetof (pkg_config_pkg_t, realname)},
  {"Description", offsetof (pkg_config_pkg_t, description)},
  {"Version", offsetof (pkg_config_pkg_t, version)},
};

static const pkg_config_parser_operand_func_t pkg_parser_funcs[] = {
  [':'] = pkg_config_pkg_parser_keyword_set,
  ['='] = pkg_config_pkg_parser_value_set};

static unsigned int
pkg_config_pkg_validate (const pkg_config_client_t* client,
                         const pkg_config_pkg_t* pkg)
{
  size_t i;
  unsigned int eflags = LIBPKG_CONFIG_ERRF_OK;

  for (i = 0; i < PKG_CONFIG_ARRAY_SIZE (pkg_config_pkg_validations); i++)
  {
    char** p = (char**)((char*)pkg + pkg_config_pkg_validations[i].offset);

    if (*p != NULL)
      continue;

    eflags = LIBPKG_CONFIG_ERRF_FILE_MISSING_FIELD;
    pkg_config_error (client,
                      eflags,
                      pkg->filename,
                      0 /* lineno */,
                      "missing '%s' field",
                      pkg_config_pkg_validations[i].field);
  }

  return eflags;
}

/*
 * !doc
 *
 * .. c:function:: pkg_config_pkg_t *pkg_config_pkg_new_from_file(const
 * pkg_config_client_t *client, const char *filename, FILE *f)
 *
 *    Parse a .pc file into a pkg_config_pkg_t object structure.
 *
 *    :param pkg_config_client_t* client: The pkg-config client object to use
 * for dependency resolution. :param char* filename: The filename of the
 * package file (including full path). :param FILE* f: The file object to read
 * from. :returns: A ``pkg_config_pkg_t`` object which contains the package
 * data. :rtype: pkg_config_pkg_t *
 */
pkg_config_pkg_t*
pkg_config_pkg_new_from_file (pkg_config_client_t* client,
                              const char* filename,
                              FILE* f,
                              unsigned int* eflags)
{
  pkg_config_pkg_t* pkg;
  char* idptr;

  pkg = calloc (1, sizeof (pkg_config_pkg_t));

  if (pkg == NULL)
  {
    *eflags = LIBPKG_CONFIG_ERRF_MEMORY;
    return NULL;
  }

  pkg->owner = client;
  pkg->filename = strdup (filename);
  pkg->pc_filedir = pkg_get_parent_dir (pkg);

  char* pc_filedir_value = convert_path_to_value (pkg->pc_filedir);
  if (pc_filedir_value == NULL)
  {
    *eflags = LIBPKG_CONFIG_ERRF_MEMORY;
    return NULL;
  }

  pkg_config_tuple_add (
      client, &pkg->vars, "pcfiledir", pc_filedir_value, true);
  free (pc_filedir_value);

  /* If pc_filedir is outside of sysroot_dir, clear pc_filedir
   * See https://github.com/pkgconf/pkgconf/issues/213
   */
  /* The implementation does not match the comment and seems wrong. But let's
     keep for posterity in case we want to fix this properly ourselves (though
     feels like if one sets sysroot but has files outside sysroot the problem
     if with the setup). */
#if 0
  if (client->sysroot_dir && strncmp(pkg->pc_filedir, client->sysroot_dir, strlen(client->sysroot_dir)))
  {
    free(client->sysroot_dir);
    client->sysroot_dir = NULL;
    pkg_config_client_set_sysroot_dir(client, NULL);
  }
#endif

  /* make module id */
  if ((idptr = strrchr (pkg->filename, LIBPKG_CONFIG_DIR_SEP_S)) != NULL)
    idptr++;
  else
    idptr = pkg->filename;

#ifdef _WIN32
  /* On Windows, both \ and / are allowed in paths, so we have to chop both.
   * strrchr() took us to the last \ in that case, so we just have to see if
   * it is followed by a /.  If so, lop it off.
   */
  char* mungeptr;
  if ((mungeptr = strrchr (idptr, '/')) != NULL)
    idptr = ++mungeptr;
#endif

  pkg->id = strdup (idptr);
  idptr = strrchr (pkg->id, '.');
  if (idptr)
    *idptr = '\0';

  *eflags = pkg_config_parser_parse (client,
                                     f,
                                     pkg,
                                     pkg_parser_funcs,
                                     PKG_CONFIG_ARRAY_SIZE (pkg_parser_funcs),
                                     pkg->filename);


  if (*eflags != LIBPKG_CONFIG_ERRF_OK ||
      (*eflags = pkg_config_pkg_validate (client, pkg)) != LIBPKG_CONFIG_ERRF_OK)
  {
    pkg_config_pkg_free (client, pkg);
    return NULL;
  }

  return pkg_config_pkg_ref (client, pkg);
}

/*
 * !doc
 *
 * .. c:function:: void pkg_config_pkg_free(pkg_config_client_t *client,
 * pkg_config_pkg_t *pkg)
 *
 *    Releases all releases for a given ``pkg_config_pkg_t`` object.
 *
 *    :param pkg_config_client_t* client: The client which owns the
 * ``pkg_config_pkg_t`` object, `pkg`. :param pkg_config_pkg_t* pkg: The
 * package to free. :return: nothing
 */
void
pkg_config_pkg_free (pkg_config_client_t* client, pkg_config_pkg_t* pkg)
{
  if (pkg == NULL)
    return;

  pkg_config_cache_remove (client, pkg);

  pkg_config_dependency_free (&pkg->required);
  pkg_config_dependency_free (&pkg->requires_private);
  pkg_config_dependency_free (&pkg->conflicts);

  pkg_config_fragment_free (&pkg->cflags);
  pkg_config_fragment_free (&pkg->cflags_private);
  pkg_config_fragment_free (&pkg->libs);
  pkg_config_fragment_free (&pkg->libs_private);

  pkg_config_tuple_free (&pkg->vars);

  if (pkg->id != NULL)
    free (pkg->id);

  if (pkg->filename != NULL)
    free (pkg->filename);

  if (pkg->realname != NULL)
    free (pkg->realname);

  if (pkg->version != NULL)
    free (pkg->version);

  if (pkg->description != NULL)
    free (pkg->description);

  if (pkg->url != NULL)
    free (pkg->url);

  if (pkg->pc_filedir != NULL)
    free (pkg->pc_filedir);

  free (pkg);
}

/*
 * !doc
 *
 * .. c:function:: pkg_config_pkg_t *pkg_config_pkg_ref(const
 * pkg_config_client_t *client, pkg_config_pkg_t *pkg)
 *
 *    Adds an additional reference to the package object (unless it is
 * static).
 *
 *    :param pkg_config_client_t* client: The pkg-config client object which
 * owns the package being referenced. :param pkg_config_pkg_t* pkg: The
 * package object being referenced. :return: The package itself with an
 * incremented reference count. :rtype: pkg_config_pkg_t *
 */
pkg_config_pkg_t*
pkg_config_pkg_ref (pkg_config_client_t* client, pkg_config_pkg_t* pkg)
{
  if (pkg->refcount >= 0)
  {
    assert ((pkg->flags & LIBPKG_CONFIG_PKG_PROPF_CONST) == 0);

    if (pkg->owner != NULL && pkg->owner != client)
      PKG_CONFIG_TRACE (
          client,
          "WTF: client %p refers to package %p owned by other client %p",
          client,
          pkg,
          pkg->owner);

    pkg->refcount++;
    PKG_CONFIG_TRACE (client, "refcount@%p: %d", pkg, pkg->refcount);
  }

  return pkg;
}

/*
 * !doc
 *
 * .. c:function:: void pkg_config_pkg_unref(pkg_config_client_t *client,
 * pkg_config_pkg_t *pkg)
 *
 *    Releases a reference on the package object (unless it is static).  If
 * the reference count is 0, then also free the package.
 *
 *    :param pkg_config_client_t* client: The pkg-config client object which
 * owns the package being dereferenced. :param pkg_config_pkg_t* pkg: The
 * package object being dereferenced. :return: nothing
 */
void
pkg_config_pkg_unref (pkg_config_client_t* client, pkg_config_pkg_t* pkg)
{
  if (pkg->refcount >= 0)
  {
    assert ((pkg->flags & LIBPKG_CONFIG_PKG_PROPF_CONST) == 0 &&
            pkg->refcount != 0);

    if (pkg->owner != NULL && pkg->owner != client)
      PKG_CONFIG_TRACE (
          client,
          "WTF: client %p unrefs package %p owned by other client %p",
          client,
          pkg,
          pkg->owner);

    pkg->refcount--;
    PKG_CONFIG_TRACE (pkg->owner, "refcount@%p: %d", pkg, pkg->refcount);

    if (pkg->refcount == 0)
      pkg_config_pkg_free (pkg->owner, pkg);
  }
}

/* If the file does not exist, return NULL and LIBPKG_CONFIG_ERRF_OK. */
static inline pkg_config_pkg_t*
pkg_config_pkg_try_specific_path (pkg_config_client_t* client,
                                  const char* path,
                                  const char* name,
                                  unsigned int* eflags)
{
  pkg_config_pkg_t* pkg = NULL;
  FILE* f;
  char locbuf[PKG_CONFIG_ITEM_SIZE];

  *eflags = LIBPKG_CONFIG_ERRF_OK;

  PKG_CONFIG_TRACE (client, "trying path: %s for %s", path, name);

  if ((client->flags & LIBPKG_CONFIG_PKG_PKGF_CONSIDER_UNINSTALLED) != 0)
  {
    snprintf (locbuf,
              sizeof locbuf,
              "%s%c%s-uninstalled" PKG_CONFIG_EXT,
              path,
              LIBPKG_CONFIG_DIR_SEP_S,
              name);

    if ((f = fopen (locbuf, "r")) != NULL)
    {
      PKG_CONFIG_TRACE (client, "found (uninstalled): %s", locbuf);
      pkg = pkg_config_pkg_new_from_file (client, locbuf, f, eflags);
      if (pkg != NULL)
        pkg->flags |= LIBPKG_CONFIG_PKG_PROPF_UNINSTALLED;
      return pkg;
    }
  }

  snprintf (locbuf,
            sizeof locbuf,
            "%s%c%s" PKG_CONFIG_EXT,
            path,
            LIBPKG_CONFIG_DIR_SEP_S,
            name);

  if ((f = fopen (locbuf, "r")) != NULL)
  {
    PKG_CONFIG_TRACE (client, "found: %s", locbuf);
    pkg = pkg_config_pkg_new_from_file (client, locbuf, f, eflags);
    return pkg;
  }

  return NULL;
}

/*
 * !doc
 *
 * .. c:function:: pkg_config_pkg_t *pkg_config_pkg_find(pkg_config_client_t
 * *client, const char *name)
 *
 *    Search for a package.
 *
 *    :param pkg_config_client_t* client: The pkg-config client object to use
 * for dependency resolution. :param char* name: The name of the package
 * `atom` to use for searching. :return: A package object reference if the
 * package was found, else ``NULL``. :rtype: pkg_config_pkg_t *
 *
 *
 * If not found, return NULL and LIBPKG_CONFIG_ERRF_OK.
 *
 */
pkg_config_pkg_t*
pkg_config_pkg_find (pkg_config_client_t* client,
                     const char* name,
                     unsigned int* eflags)
{
  pkg_config_pkg_t* pkg = NULL;
  pkg_config_node_t* n;
  FILE* f;

  *eflags = LIBPKG_CONFIG_ERRF_OK;

  PKG_CONFIG_TRACE (client, "looking for: %s", name);

  /* name might actually be a filename. */
  if (str_has_suffix (name, PKG_CONFIG_EXT))
  {
    if ((f = fopen (name, "r")) != NULL)
    {
      PKG_CONFIG_TRACE (client, "%s is a file", name);

      pkg = pkg_config_pkg_new_from_file (client, name, f, eflags);
      if (pkg != NULL)
        pkg_config_path_add (pkg->pc_filedir, &client->dir_list, true);
    }

    /* There is no point in trying anything else since the name contains the
       extension. */
    return pkg;
  }

  /* check builtins */
  if ((pkg = (pkg_config_pkg_t*)pkg_config_builtin_pkg_get (name)) != NULL)
  {
    PKG_CONFIG_TRACE (client, "%s is a builtin", name);
    return pkg;
  }

  /* check cache */
  if (!(client->flags & LIBPKG_CONFIG_PKG_PKGF_NO_CACHE))
  {
    if ((pkg = pkg_config_cache_lookup (client, name)) != NULL)
    {
      PKG_CONFIG_TRACE (client, "%s is cached", name);
      return pkg;
    }
  }

  LIBPKG_CONFIG_FOREACH_LIST_ENTRY (client->dir_list.head, n)
  {
    pkg_config_path_t* pnode = n->data;

    pkg = pkg_config_pkg_try_specific_path (client, pnode->path, name, eflags);
    if (pkg != NULL || *eflags != LIBPKG_CONFIG_ERRF_OK)
      break;
  }

  if (pkg != NULL)
    pkg_config_cache_add (client, pkg);

  return pkg;
}

/*
 * !doc
 *
 * .. c:function:: int pkg_config_compare_version(const char *a, const char
 * *b)
 *
 *    Compare versions using RPM version comparison rules as described in the
 * LSB.
 *
 *    :param char* a: The first version to compare in the pair.
 *    :param char* b: The second version to compare in the pair.
 *    :return: -1 if the first version is greater, 0 if both versions are
 * equal, 1 if the second version is greater. :rtype: int
 */
int
pkg_config_compare_version (const char* a, const char* b)
{
  char oldch1, oldch2;
  char buf1[PKG_CONFIG_ITEM_SIZE], buf2[PKG_CONFIG_ITEM_SIZE];
  char *str1, *str2;
  char *one, *two;
  int ret;
  bool isnum;

  /* optimization: if version matches then it's the same version. */
  if (a == NULL)
    return 1;

  if (b == NULL)
    return -1;

  if (!strcasecmp (a, b))
    return 0;

  pkg_config_strlcpy (buf1, a, sizeof buf1);
  pkg_config_strlcpy (buf2, b, sizeof buf2);

  one = str1 = buf1;
  two = str2 = buf2;

  while (*one || *two)
  {
    while (*one && !isalnum ((unsigned int)*one) && *one != '~') one++;
    while (*two && !isalnum ((unsigned int)*two) && *two != '~') two++;

    if (*one == '~' || *two == '~')
    {
      if (*one != '~')
        return -1;
      if (*two != '~')
        return 1;

      one++;
      two++;
      continue;
    }

    if (!(*one && *two))
      break;

    str1 = one;
    str2 = two;

    if (isdigit ((unsigned int)*str1))
    {
      while (*str1 && isdigit ((unsigned int)*str1)) str1++;

      while (*str2 && isdigit ((unsigned int)*str2)) str2++;

      isnum = true;
    }
    else
    {
      while (*str1 && isalpha ((unsigned int)*str1)) str1++;

      while (*str2 && isalpha ((unsigned int)*str2)) str2++;

      isnum = false;
    }

    oldch1 = *str1;
    oldch2 = *str2;

    *str1 = '\0';
    *str2 = '\0';

    if (one == str1)
      return -1;

    if (two == str2)
      return (isnum ? 1 : -1);

    if (isnum)
    {
      int onelen, twolen;

      while (*one == '0') one++;

      while (*two == '0') two++;

      onelen = strlen (one);
      twolen = strlen (two);

      if (onelen > twolen)
        return 1;
      else if (twolen > onelen)
        return -1;
    }

    ret = strcmp (one, two);
    if (ret != 0)
      return ret < 0 ? -1 : 1;

    *str1 = oldch1;
    *str2 = oldch2;

    one = str1;
    two = str2;
  }

  if ((!*one) && (!*two))
    return 0;

  if (!*one)
    return -1;

  return 1;
}

static const pkg_config_pkg_t pkg_config_virtual = {
    .refcount = -1, /* Static. */
    .id = "pkg-config",
    .realname = "pkg-config",
    .description = "virtual pkg-config package",
    .url = LIBPKG_CONFIG_PROJECT_URL,
    .version = LIBPKG_CONFIG_VERSION_ID,
    .flags = LIBPKG_CONFIG_PKG_PROPF_CONST,
    .vars = {
    .head = &(pkg_config_node_t) {
      .next = &(pkg_config_node_t) {
        .next = &(pkg_config_node_t) {
          .data = &(pkg_config_tuple_t) {
            .key = "pc_system_libdirs",
            .value = SYSTEM_LIBDIR,
          }},
        .data = &(pkg_config_tuple_t) {
          .key = "pc_system_includedirs",
          .value = SYSTEM_INCLUDEDIR,
        }},
      .data = &(pkg_config_tuple_t) {
        .key = "pc_path",
        .value = PKG_CONFIG_DEFAULT_PATH,
      },
    },
    .tail = NULL,
  }};

typedef struct
{
  const char* name;
  const pkg_config_pkg_t* pkg;
} pkg_config_builtin_pkg_pair_t;

/* keep these in alphabetical order */
static const pkg_config_builtin_pkg_pair_t pkg_config_builtin_pkg_pair_set[] =
{
  {"pkg-config", &pkg_config_virtual}
};

static int
pkg_config_builtin_pkg_pair_cmp (const void* key, const void* ptr)
{
  const pkg_config_builtin_pkg_pair_t* pair = ptr;
  return strcasecmp (key, pair->name);
}

/*
 * !doc
 *
 * .. c:function:: pkg_config_pkg_t *pkg_config_builtin_pkg_get(const char
 * *name)
 *
 *    Looks up a built-in package.  The package should not be freed or
 * dereferenced.
 *
 *    :param char* name: An atom corresponding to a built-in package to search
 * for. :return: the built-in package if present, else ``NULL``. :rtype:
 * pkg_config_pkg_t *
 */
const pkg_config_pkg_t*
pkg_config_builtin_pkg_get (const char* name)
{
  const pkg_config_builtin_pkg_pair_t* pair =
      bsearch (name,
               pkg_config_builtin_pkg_pair_set,
               PKG_CONFIG_ARRAY_SIZE (pkg_config_builtin_pkg_pair_set),
               sizeof (pkg_config_builtin_pkg_pair_t),
               pkg_config_builtin_pkg_pair_cmp);

  return (pair != NULL) ? pair->pkg : NULL;
}

typedef bool (*pkg_config_vercmp_res_func_t) (const char* a, const char* b);

typedef struct
{
  const char* name;
  pkg_config_pkg_comparator_t compare;
} pkg_config_pkg_comparator_pair_t;

static const pkg_config_pkg_comparator_pair_t
pkg_config_pkg_comparator_names[] = {
  {"!=", PKG_CONFIG_CMP_NOT_EQUAL},
  {"(any)", PKG_CONFIG_CMP_ANY},
  {"<", PKG_CONFIG_CMP_LESS_THAN},
  {"<=", PKG_CONFIG_CMP_LESS_THAN_EQUAL},
  {"=", PKG_CONFIG_CMP_EQUAL},
  {">", PKG_CONFIG_CMP_GREATER_THAN},
  {">=", PKG_CONFIG_CMP_GREATER_THAN_EQUAL}};

static int
pkg_config_pkg_comparator_pair_namecmp (const void* key, const void* ptr)
{
  const pkg_config_pkg_comparator_pair_t* pair = ptr;
  return strcmp (key, pair->name);
}

static bool
pkg_config_pkg_comparator_lt (const char* a, const char* b)
{
  return (pkg_config_compare_version (a, b) < 0);
}

static bool
pkg_config_pkg_comparator_gt (const char* a, const char* b)
{
  return (pkg_config_compare_version (a, b) > 0);
}

static bool
pkg_config_pkg_comparator_lte (const char* a, const char* b)
{
  return (pkg_config_compare_version (a, b) <= 0);
}

static bool
pkg_config_pkg_comparator_gte (const char* a, const char* b)
{
  return (pkg_config_compare_version (a, b) >= 0);
}

static bool
pkg_config_pkg_comparator_eq (const char* a, const char* b)
{
  return (pkg_config_compare_version (a, b) == 0);
}

static bool
pkg_config_pkg_comparator_ne (const char* a, const char* b)
{
  return (pkg_config_compare_version (a, b) != 0);
}

static bool
pkg_config_pkg_comparator_any (const char* a, const char* b)
{
  (void)a;
  (void)b;

  return true;
}

static const pkg_config_vercmp_res_func_t pkg_config_pkg_comparator_impls[] =
{
  [PKG_CONFIG_CMP_ANY] = pkg_config_pkg_comparator_any,
  [PKG_CONFIG_CMP_LESS_THAN] = pkg_config_pkg_comparator_lt,
  [PKG_CONFIG_CMP_GREATER_THAN] = pkg_config_pkg_comparator_gt,
  [PKG_CONFIG_CMP_LESS_THAN_EQUAL] = pkg_config_pkg_comparator_lte,
  [PKG_CONFIG_CMP_GREATER_THAN_EQUAL] = pkg_config_pkg_comparator_gte,
  [PKG_CONFIG_CMP_EQUAL] = pkg_config_pkg_comparator_eq,
  [PKG_CONFIG_CMP_NOT_EQUAL] = pkg_config_pkg_comparator_ne
};

/*
 * !doc
 *
 * .. c:function:: const char *pkg_config_pkg_get_comparator(const
 * pkg_config_dependency_t *pkgdep)
 *
 *    Returns the comparator used in a depgraph dependency node as a string.
 *
 *    :param pkg_config_dependency_t* pkgdep: The depgraph dependency node to
 * return the comparator for. :return: A string matching the comparator or
 * ``"???"``. :rtype: char *
 */
const char*
pkg_config_pkg_get_comparator (const pkg_config_dependency_t* pkgdep)
{
  if (pkgdep->compare >=
      PKG_CONFIG_ARRAY_SIZE (pkg_config_pkg_comparator_names))
    return "???";

  return pkg_config_pkg_comparator_names[pkgdep->compare].name;
}

/*
 * !doc
 *
 * .. c:function:: pkg_config_pkg_comparator_t
 * pkg_config_pkg_comparator_lookup_by_name(const char *name)
 *
 *    Look up the appropriate comparator bytecode in the comparator set
 * (defined in ``pkg.c``, see ``pkg_config_pkg_comparator_names`` and
 * ``pkg_config_pkg_comparator_impls``).
 *
 *    :param char* name: The comparator to look up by `name`.
 *    :return: The comparator bytecode if found, else ``PKG_CONFIG_CMP_ANY``.
 *    :rtype: pkg_config_pkg_comparator_t
 */
pkg_config_pkg_comparator_t
pkg_config_pkg_comparator_lookup_by_name (const char* name)
{
  const pkg_config_pkg_comparator_pair_t* p =
      bsearch (name,
               pkg_config_pkg_comparator_names,
               PKG_CONFIG_ARRAY_SIZE (pkg_config_pkg_comparator_names),
               sizeof (pkg_config_pkg_comparator_pair_t),
               pkg_config_pkg_comparator_pair_namecmp);

  return (p != NULL) ? p->compare : PKG_CONFIG_CMP_ANY;
}

/*
 * !doc
 *
 * .. c:function:: pkg_config_pkg_t
 * *pkg_config_pkg_verify_dependency(pkg_config_client_t *client,
 * pkg_config_dependency_t *pkgdep, unsigned int *eflags)
 *
 *    Verify a pkg_config_dependency_t node in the depgraph.  If the
 * dependency is solvable, return the appropriate ``pkg_config_pkg_t`` object,
 * else
 * ``NULL``.
 *
 *    :param pkg_config_client_t* client: The pkg-config client object to use
 * for dependency resolution. :param pkg_config_dependency_t* pkgdep: The
 * dependency graph node to solve. :param uint* eflags: An optional pointer
 * that, if set, will be populated with an error code from the resolver.
 * :return: On success, the appropriate ``pkg_config_pkg_t`` object to solve
 * the dependency, else ``NULL``. :rtype: pkg_config_pkg_t *
 */
pkg_config_pkg_t*
pkg_config_pkg_verify_dependency (pkg_config_client_t* client,
                                  pkg_config_dependency_t* pkgdep,
                                  unsigned int* eflags)
{
  pkg_config_pkg_t* pkg = NULL;

  if (eflags != NULL)
    *eflags = LIBPKG_CONFIG_ERRF_OK;

  PKG_CONFIG_TRACE (
      client, "trying to verify dependency: %s", pkgdep->package);

  if (pkgdep->match != NULL)
  {
    PKG_CONFIG_TRACE (client,
                      "cached dependency: %s -> %s@%p",
                      pkgdep->package,
                      pkgdep->match->id,
                      pkgdep->match);
    return pkg_config_pkg_ref (client, pkgdep->match);
  }

  unsigned int def;
  pkg = pkg_config_pkg_find (client, pkgdep->package, &def);
  if (pkg == NULL)
  {
    if (eflags != NULL)
      *eflags |= (def == LIBPKG_CONFIG_ERRF_OK
                  ? LIBPKG_CONFIG_ERRF_PACKAGE_NOT_FOUND
                  : LIBPKG_CONFIG_ERRF_PACKAGE_INVALID);
    return NULL;
  }

  if (pkg->id == NULL)
  {
    assert ((pkg->flags & LIBPKG_CONFIG_PKG_PROPF_CONST) == 0);
    pkg->id = strdup (pkgdep->package);
  }

  if (pkg_config_pkg_comparator_impls[pkgdep->compare](
          pkg->version, pkgdep->version) != true)
  {
    if (eflags != NULL)
      *eflags |= LIBPKG_CONFIG_ERRF_PACKAGE_VER_MISMATCH;
  }
  else
    pkgdep->match = pkg_config_pkg_ref (client, pkg);

  return pkg;
}

/*
 * !doc
 *
 * .. c:function:: unsigned int
 * pkg_config_pkg_verify_graph(pkg_config_client_t *client, pkg_config_pkg_t
 * *root, int depth)
 *
 *    Verify the graph dependency nodes are satisfiable by walking the tree
 * using
 *    ``pkg_config_pkg_traverse()``.
 *
 *    :param pkg_config_client_t* client: The pkg-config client object to use
 * for dependency resolution. :param pkg_config_pkg_t* root: The root entry in
 * the package dependency graph which should contain the top-level
 * dependencies to resolve. :param int depth: The maximum allowed depth for
 * dependency resolution. :return: On success, ``LIBPKG_CONFIG_ERRF_OK``
 * (0), else an error code. :rtype: unsigned int
 */
unsigned int
pkg_config_pkg_verify_graph (pkg_config_client_t* client,
                             pkg_config_pkg_t* root,
                             int depth)
{
  return pkg_config_pkg_traverse (client, root, NULL, NULL, depth, 0);
}

static unsigned int
pkg_config_pkg_report_graph_error (pkg_config_client_t* client,
                                   const pkg_config_pkg_t* parent,
                                   pkg_config_pkg_t* pkg,
                                   const pkg_config_dependency_t* node,
                                   unsigned int eflags)
{
  if (eflags & LIBPKG_CONFIG_ERRF_PACKAGE_NOT_FOUND)
  {
    pkg_config_error (client,
                      LIBPKG_CONFIG_ERRF_PACKAGE_NOT_FOUND,
                      NULL, 0,
                      "package '%s' required by '%s' not found",
                      node->package,
                      parent->id);
    /*
    pkg_config_audit_log(client, "%s NOT-FOUND\n", node->package);
    */
  }
  else if (eflags & LIBPKG_CONFIG_ERRF_PACKAGE_INVALID)
  {
    pkg_config_error (client,
                      LIBPKG_CONFIG_ERRF_PACKAGE_INVALID,
                      NULL, 0,
                      "package '%s' required by '%s' found but invalid",
                      node->package,
                      parent->id);
  }
  else if (eflags & LIBPKG_CONFIG_ERRF_PACKAGE_VER_MISMATCH)
  {
    if (pkg == NULL)
    {
      pkg_config_error (
          client,
          LIBPKG_CONFIG_ERRF_PACKAGE_VER_MISMATCH,
          NULL, 0,
          "package version constraint '%s %s %s' could not be satisfied",
          node->package,
          pkg_config_pkg_get_comparator (node),
          node->version);
    }
    else
    {
      pkg_config_error (
          client,
          LIBPKG_CONFIG_ERRF_PACKAGE_VER_MISMATCH,
          NULL, 0,
          "package version constraint '%s %s %s' could not be satisfied, "
          "available version is '%s'",
          node->package,
          pkg_config_pkg_get_comparator (node),
          node->version,
          pkg->version);
    }
  }

  if (pkg != NULL)
    pkg_config_pkg_unref (client, pkg);

  return eflags;
}

static inline unsigned int
pkg_config_pkg_walk_list (pkg_config_client_t* client,
                          pkg_config_pkg_t* parent,
                          pkg_config_list_t* deplist,
                          pkg_config_pkg_traverse_func_t func,
                          void* data,
                          int depth,
                          unsigned int skip_flags)
{
  unsigned int eflags = LIBPKG_CONFIG_ERRF_OK;
  pkg_config_node_t* node;

  LIBPKG_CONFIG_FOREACH_LIST_ENTRY (deplist->head, node)
  {
    unsigned int eflags_local = LIBPKG_CONFIG_ERRF_OK;
    pkg_config_dependency_t* depnode = node->data;
    pkg_config_pkg_t* pkgdep;

    if (*depnode->package == '\0')
      continue;

    pkgdep =
        pkg_config_pkg_verify_dependency (client, depnode, &eflags_local);

    eflags |= eflags_local;
    if (eflags_local != LIBPKG_CONFIG_ERRF_OK &&
        !(client->flags & LIBPKG_CONFIG_PKG_PKGF_SKIP_ERRORS))
    {
      pkg_config_pkg_report_graph_error (
          client, parent, pkgdep, depnode, eflags_local);
      continue;
    }
    if (pkgdep == NULL)
      continue;

    if (pkgdep->flags & LIBPKG_CONFIG_PKG_PROPF_SEEN)
    {
      pkg_config_pkg_unref (client, pkgdep);
      continue;
    }

    if (skip_flags && (depnode->flags & skip_flags) == skip_flags)
    {
      pkg_config_pkg_unref (client, pkgdep);
      continue;
    }

    /*
    pkg_config_audit_log_dependency(client, pkgdep, depnode);
    */

    if ((pkgdep->flags & LIBPKG_CONFIG_PKG_PROPF_CONST) == 0)
      pkgdep->flags |= LIBPKG_CONFIG_PKG_PROPF_SEEN;

    eflags |= pkg_config_pkg_traverse (
        client, pkgdep, func, data, depth - 1, skip_flags);

    if ((pkgdep->flags & LIBPKG_CONFIG_PKG_PROPF_CONST) == 0)
      pkgdep->flags &= ~LIBPKG_CONFIG_PKG_PROPF_SEEN;

    pkg_config_pkg_unref (client, pkgdep);
  }

  return eflags;
}

static inline unsigned int
pkg_config_pkg_walk_conflicts_list (pkg_config_client_t* client,
                                    const pkg_config_pkg_t* root,
                                    const pkg_config_list_t* deplist)
{
  unsigned int eflags;
  pkg_config_node_t *node, *childnode;

  LIBPKG_CONFIG_FOREACH_LIST_ENTRY (deplist->head, node)
  {
    pkg_config_dependency_t* parentnode = node->data;

    if (*parentnode->package == '\0')
      continue;

    LIBPKG_CONFIG_FOREACH_LIST_ENTRY (root->required.head, childnode)
    {
      pkg_config_pkg_t* pkgdep;
      pkg_config_dependency_t* depnode = childnode->data;

      if (*depnode->package == '\0' ||
          strcmp (depnode->package, parentnode->package))
        continue;

      pkgdep = pkg_config_pkg_verify_dependency (client, parentnode, &eflags);
      if (eflags == LIBPKG_CONFIG_ERRF_OK)
      {
        pkg_config_error (client,
                          LIBPKG_CONFIG_ERRF_PACKAGE_CONFLICT,
                          NULL, 0,
                          "version '%s' of '%s' conflicts with '%s' due to "
                          "conflict rule '%s %s%s%s'",
                          pkgdep->version,
                          pkgdep->realname,
                          root->realname,
                          parentnode->package,
                          pkg_config_pkg_get_comparator (parentnode),
                          parentnode->version != NULL ? " " : "",
                          parentnode->version != NULL ? parentnode->version
                                                      : "");

        pkg_config_pkg_unref (client, pkgdep);

        return LIBPKG_CONFIG_ERRF_PACKAGE_CONFLICT;
      }

      pkg_config_pkg_unref (client, pkgdep);
    }
  }

  return LIBPKG_CONFIG_ERRF_OK;
}

/*
 * !doc
 *
 * .. c:function:: unsigned int pkg_config_pkg_traverse(pkg_config_client_t
 * *client, pkg_config_pkg_t *root, pkg_config_pkg_traverse_func_t func, void
 * *data, int maxdepth, unsigned int skip_flags)
 *
 *    Walk and resolve the dependency graph up to `maxdepth` levels.
 *
 *    :param pkg_config_client_t* client: The pkg-config client object to use
 * for dependency resolution. :param pkg_config_pkg_t* root: The root of the
 * dependency graph. :param pkg_config_pkg_traverse_func_t func: A traversal
 * function to call for each resolved node in the dependency graph. :param
 * void* data: An opaque pointer to data to be passed to the traversal
 * function. :param int maxdepth: The maximum depth to walk the dependency
 * graph for.  -1 means infinite recursion. :param uint skip_flags: Skip over
 * dependency nodes containing the specified flags.  A setting of 0 skips no
 * dependency nodes. :return: ``LIBPKG_CONFIG_ERRF_OK`` on success, else
 * an error code. :rtype: unsigned int
 */
unsigned int
pkg_config_pkg_traverse (pkg_config_client_t* client,
                         pkg_config_pkg_t* root,
                         pkg_config_pkg_traverse_func_t func,
                         void* data,
                         int maxdepth,
                         unsigned int skip_flags)
{
  unsigned int eflags = LIBPKG_CONFIG_ERRF_OK;

  if (maxdepth == 0)
    return eflags;

  PKG_CONFIG_TRACE (client, "%s: level %d", root->id, maxdepth);

  if (func != NULL)
    func (client, root, data);

  if (!(client->flags & LIBPKG_CONFIG_PKG_PKGF_SKIP_CONFLICTS))
  {
    eflags =
        pkg_config_pkg_walk_conflicts_list (client, root, &root->conflicts);
    if (eflags != LIBPKG_CONFIG_ERRF_OK)
      return eflags;
  }

  PKG_CONFIG_TRACE (client, "%s: walking requires list", root->id);
  eflags = pkg_config_pkg_walk_list (
      client, root, &root->required, func, data, maxdepth, skip_flags);
  if (eflags != LIBPKG_CONFIG_ERRF_OK)
    return eflags;

  if (client->flags & LIBPKG_CONFIG_PKG_PKGF_SEARCH_PRIVATE)
  {
    PKG_CONFIG_TRACE (client, "%s: walking requires.private list", root->id);

    /* XXX: ugly */
    client->flags |= LIBPKG_CONFIG_PKG_PKGF_ITER_PKG_IS_PRIVATE;
    eflags = pkg_config_pkg_walk_list (client,
                                       root,
                                       &root->requires_private,
                                       func,
                                       data,
                                       maxdepth,
                                       skip_flags);
    client->flags &= ~LIBPKG_CONFIG_PKG_PKGF_ITER_PKG_IS_PRIVATE;

    if (eflags != LIBPKG_CONFIG_ERRF_OK)
      return eflags;
  }

  return eflags;
}

static void
pkg_config_pkg_cflags_collect (pkg_config_client_t* client,
                               pkg_config_pkg_t* pkg,
                               void* data)
{
  pkg_config_list_t* list = data;
  pkg_config_node_t* node;

  LIBPKG_CONFIG_FOREACH_LIST_ENTRY (pkg->cflags.head, node)
  {
    pkg_config_fragment_t* frag = node->data;
    pkg_config_fragment_copy (client, list, frag, false);
  }
}

static void
pkg_config_pkg_cflags_private_collect (pkg_config_client_t* client,
                                       pkg_config_pkg_t* pkg,
                                       void* data)
{
  pkg_config_list_t* list = data;
  pkg_config_node_t* node;

  LIBPKG_CONFIG_FOREACH_LIST_ENTRY (pkg->cflags_private.head, node)
  {
    pkg_config_fragment_t* frag = node->data;
    pkg_config_fragment_copy (client, list, frag, true);
  }
}

/*
 * !doc
 *
 * .. c:function:: int pkg_config_pkg_cflags(pkg_config_client_t *client,
 * pkg_config_pkg_t *root, pkg_config_list_t *list, int maxdepth)
 *
 *    Walks a dependency graph and extracts relevant ``CFLAGS`` fragments.
 *
 *    :param pkg_config_client_t* client: The pkg-config client object to use
 * for dependency resolution. :param pkg_config_pkg_t* root: The root of the
 * dependency graph. :param pkg_config_list_t* list: The fragment list to add
 * the extracted ``CFLAGS`` fragments to. :param int maxdepth: The maximum
 * allowed depth for dependency resolution.  -1 means infinite recursion.
 * :return:
 * ``LIBPKG_CONFIG_ERRF_OK`` if successful, otherwise an error code.
 *    :rtype: unsigned int
 */
unsigned int
pkg_config_pkg_cflags (pkg_config_client_t* client,
                       pkg_config_pkg_t* root,
                       pkg_config_list_t* list,
                       int maxdepth)
{
  unsigned int eflag;
  unsigned int skip_flags =
      (client->flags & LIBPKG_CONFIG_PKG_PKGF_DONT_FILTER_INTERNAL_CFLAGS) ==
              0
          ? LIBPKG_CONFIG_PKG_DEPF_INTERNAL
          : 0;
  pkg_config_list_t frags = LIBPKG_CONFIG_LIST_INITIALIZER;

  eflag = pkg_config_pkg_traverse (client,
                                   root,
                                   pkg_config_pkg_cflags_collect,
                                   &frags,
                                   maxdepth,
                                   skip_flags);

  if (eflag == LIBPKG_CONFIG_ERRF_OK &&
      client->flags & LIBPKG_CONFIG_PKG_PKGF_ADD_PRIVATE_FRAGMENTS)
    eflag = pkg_config_pkg_traverse (client,
                                     root,
                                     pkg_config_pkg_cflags_private_collect,
                                     &frags,
                                     maxdepth,
                                     skip_flags);

  if (eflag != LIBPKG_CONFIG_ERRF_OK)
  {
    pkg_config_fragment_free (&frags);
    return eflag;
  }

  pkg_config_fragment_copy_list (client, list, &frags);
  pkg_config_fragment_free (&frags);

  return eflag;
}

static void
pkg_config_pkg_libs_collect (pkg_config_client_t* client,
                             pkg_config_pkg_t* pkg,
                             void* data)
{
  pkg_config_list_t* list = data;
  pkg_config_node_t* node;

  LIBPKG_CONFIG_FOREACH_LIST_ENTRY (pkg->libs.head, node)
  {
    pkg_config_fragment_t* frag = node->data;
    pkg_config_fragment_copy (
        client,
        list,
        frag,
        (client->flags & LIBPKG_CONFIG_PKG_PKGF_ITER_PKG_IS_PRIVATE) != 0);
  }

  if (client->flags & LIBPKG_CONFIG_PKG_PKGF_ADD_PRIVATE_FRAGMENTS)
  {
    LIBPKG_CONFIG_FOREACH_LIST_ENTRY (pkg->libs_private.head, node)
    {
      pkg_config_fragment_t* frag = node->data;
      pkg_config_fragment_copy (client, list, frag, true);
    }
  }
}

/*
 * !doc
 *
 * .. c:function:: int pkg_config_pkg_libs(pkg_config_client_t *client,
 * pkg_config_pkg_t *root, pkg_config_list_t *list, int maxdepth)
 *
 *    Walks a dependency graph and extracts relevant ``LIBS`` fragments.
 *
 *    :param pkg_config_client_t* client: The pkg-config client object to use
 * for dependency resolution. :param pkg_config_pkg_t* root: The root of the
 * dependency graph. :param pkg_config_list_t* list: The fragment list to add
 * the extracted ``LIBS`` fragments to. :param int maxdepth: The maximum
 * allowed depth for dependency resolution.  -1 means infinite recursion.
 * :return:
 * ``LIBPKG_CONFIG_ERRF_OK`` if successful, otherwise an error code.
 *    :rtype: unsigned int
 */
unsigned int
pkg_config_pkg_libs (pkg_config_client_t* client,
                     pkg_config_pkg_t* root,
                     pkg_config_list_t* list,
                     int maxdepth)
{
  unsigned int eflag;

  eflag = pkg_config_pkg_traverse (
      client, root, pkg_config_pkg_libs_collect, list, maxdepth, 0);

  if (eflag != LIBPKG_CONFIG_ERRF_OK)
  {
    pkg_config_fragment_free (list);
    return eflag;
  }

  return eflag;
}
