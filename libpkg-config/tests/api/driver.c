/* file      : tests/api/driver.c
 * license   : ISC; see accompanying COPYING file
 */

/* Enable assertions.
 */
#ifdef NDEBUG
#  undef NDEBUG
#endif

#include <libpkg-config/pkg-config.h>

#include <stdio.h>   /* printf(), fprintf(), stderr */
#include <stddef.h>  /* size_t, NULL */
#include <assert.h>
#include <string.h>  /* strcmp(), strlen() */
#include <stdbool.h> /* bool, true, false */

static void
diag_handler (unsigned int e,
              const char* file,
              size_t line,
              const char* msg,
              const pkg_config_client_t* c,
              const void* d)
{
  (void) c; /* Unused. */
  (void) d; /* Unused. */

  const char* w = (e == LIBPKG_CONFIG_ERRF_OK ? "warning" : "error");

  if (file != NULL)
    fprintf (stderr,
             "%s:" LIBPKG_CONFIG_SIZE_FMT ": %s: %s\n",
             file, line,
             w,
             msg);
  else
    fprintf (stderr, "%s: %s\n", w, msg);
}

static void
frags_print_and_free (pkg_config_list_t* list)
{
  pkg_config_node_t *node;
  LIBPKG_CONFIG_FOREACH_LIST_ENTRY(list->head, node)
  {
    pkg_config_fragment_t* frag = node->data;
    printf("%c %s\n", frag->type != '\0' ? frag->type : ' ', frag->data);
  }

  pkg_config_fragment_free (list);
}

static void
tuples_print (pkg_config_list_t *list)
{
  pkg_config_node_t *node;
  LIBPKG_CONFIG_FOREACH_LIST_ENTRY(list->head, node)
  {
    pkg_config_tuple_t *tuple = node->data;

    /* Skip the automatically added variable.
     */
    if (strcmp (tuple->key, "pcfiledir") != 0)
      printf("%s %s\n", tuple->key, tuple->value);
  }
}

/* Usage: argv[0] (--cflags|--libs|--vars) <path>
 *
 * Print package compiler flags, linker flags or variable name/values one per
 * line. The specified package file must have .pc extension.
 *
 * --cflags
 *     Print compiler flags in the '<name> <value>' format.
 *
 * --libs
 *     Print linker flags in the '<name> <value>' format.
 *
 * --vars
 *     Print variables in the '<name> <value>' format.
 */
int
main (int argc, const char* argv[])
{
  enum
  {
    dump_none,
    dump_cflags,
    dump_libs,
    dump_vars
  } mode = dump_none;

  int i = 1;
  for (; i < argc; ++i)
  {
    const char* o = argv[i];

    if (strcmp (o, "--cflags") == 0)
    {
      assert (mode == dump_none);
      mode = dump_cflags;
    }
    else if (strcmp (o, "--libs") == 0)
    {
      assert (mode == dump_none);
      mode = dump_libs;
    }
    else if (strcmp (o, "--vars") == 0)
    {
      assert (mode == dump_none);
      mode = dump_vars;
    }
    else
      break;
  }

  assert (mode != dump_none);

  assert (i + 1 == argc);
  const char* path = argv[i];

  /* Make sure the file has .pc extension.
   */
  size_t n = strlen (path);
  assert (n > 3 && strcmp (path + n - 3, ".pc") == 0);

  pkg_config_client_t cs;
  pkg_config_client_t* c = &cs;
  pkg_config_client_init (c,
                          diag_handler,
                          NULL /* error_handler_data */,
                          true /* init_filters */);
  pkg_config_client_set_warn_handler (c,
                                      diag_handler,
                                      NULL /* warn_handler_data */);

  assert (c != NULL);

  int r = 1;
  int max_depth = 2000;

  const int pkg_config_flags = 0;

  pkg_config_client_set_flags (c, pkg_config_flags);

  unsigned int e;
  pkg_config_pkg_t* p = pkg_config_pkg_find (c, path, &e);

  if (p != NULL)
  {
    switch (mode)
    {
    case dump_cflags:
      {
        pkg_config_client_set_flags (c,
                                     pkg_config_flags |
                                     LIBPKG_CONFIG_PKG_PKGF_SEARCH_PRIVATE);

        pkg_config_list_t list = LIBPKG_CONFIG_LIST_INITIALIZER;
        e = pkg_config_pkg_cflags (c, p, &list, max_depth);

        if (e == LIBPKG_CONFIG_ERRF_OK)
          frags_print_and_free (&list);

        pkg_config_client_set_flags (c, 0); /* Restore. */
        break;
      }
    case dump_libs:
      {
        pkg_config_client_set_flags (
          c,
          pkg_config_flags                      |
          LIBPKG_CONFIG_PKG_PKGF_SEARCH_PRIVATE |
          LIBPKG_CONFIG_PKG_PKGF_ADD_PRIVATE_FRAGMENTS);

        pkg_config_list_t list = LIBPKG_CONFIG_LIST_INITIALIZER;
        e = pkg_config_pkg_libs (c, p, &list, max_depth);

        if (e == LIBPKG_CONFIG_ERRF_OK)
          frags_print_and_free (&list);

        pkg_config_client_set_flags (c, 0); /* Restore. */
        break;
      }
    case dump_vars:
      {
        tuples_print (&p->vars);
        break;
      }
    default:
      {
        assert (false);
      }
    }

    if (e == LIBPKG_CONFIG_ERRF_OK)
      r = 0;

    pkg_config_pkg_unref (c, p);
  }
  else if (e == LIBPKG_CONFIG_ERRF_OK)
    fprintf (stderr, "package file '%s' not found\n", path);
  else
    /* Diagnostics should have already been issue via the error handler
       except for allocation errors. */
    fprintf (stderr, "unable to load package file '%s'\n", path);

  pkg_config_client_deinit (c);
  return r;
}
