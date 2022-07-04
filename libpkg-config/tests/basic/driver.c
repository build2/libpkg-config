/* file      : tests/basic/driver.c
 * license   : ISC; see accompanying COPYING file
 */

/* Enable assertions.
 */
#ifdef NDEBUG
#  undef NDEBUG
#endif

#include <libpkg-config/pkg-config.h>

#include <stdio.h>   /* printf(), fprintf(), stderr */
#include <stddef.h>  /* NULL */
#include <stdlib.h>  /* free() */
#include <assert.h>
#include <string.h>  /* strcmp() */
#include <stdbool.h> /* bool, true, false */

static void
error_handler (unsigned int e,
               const char* msg,
               const pkgconf_client_t* c,
               const void* d)
{
  (void) e; /* Unused. */
  (void) c; /* Unused. */
  (void) d; /* Unused. */

  fprintf (stderr, "%s\n", msg);
}

static void
print_and_free (pkgconf_list_t* list)
{
  char* buf = pkgconf_fragment_render (list,
                                       true /* escape */,
                                       NULL /* options */);
  printf("%s", buf);
  free (buf);

  pkgconf_fragment_free (list);
}

/* Usage: argv[0] [--cflags] [--libs] (--with-path <dir>)* <name>
 *
 * Print package compiler and linker flags. If the package name has '.pc'
 * extension it is interpreted as a file name. Prints all flags, as pkgconf
 * utility does when --keep-system-libs and --keep-system-cflags are specified.
 *
 * --cflags
 *     Print compiler flags.
 *
 * --libs
 *     Print linker flags.
 *
 * --static
 *     Assume static linking.
 *
 * --with-path <dir>
 *     Search through the directory for pc-files. If at least one --with-path
 *     is specified then the default directories are not searched through.
 */
int
main (int argc, const char* argv[])
{
  pkgconf_client_t* c = pkgconf_client_new (error_handler,
                                            NULL /* error_handler_data */,
                                            true /* init_filters */);

  assert (c != NULL);

  bool cflags = false;
  bool libs = false;
  bool default_dirs = true;
  int client_flags = LIBPKG_CONFIG_PKG_PKGF_MERGE_SPECIAL_FRAGMENTS;

  int i = 1;
  for (; i < argc; ++i)
  {
    const char* o = argv[i];

    if (strcmp (o, "--cflags") == 0)
      cflags = true;
    else if (strcmp (o, "--libs") == 0)
      libs = true;
    else if (strcmp (o, "--static") == 0)
      client_flags |= LIBPKG_CONFIG_PKG_PKGF_SEARCH_PRIVATE |
                      LIBPKG_CONFIG_PKG_PKGF_MERGE_PRIVATE_FRAGMENTS;
    else if (strcmp (o, "--with-path") == 0)
    {
      ++i;
      assert (i < argc);

      pkgconf_path_add (argv[i], &c->dir_list, true /* filter_duplicates */);
      default_dirs = false;
    }
    else
      break;
  }

  assert (i + 1 == argc);
  const char* name = argv[i];

  int r = 1;
  int max_depth = 2000;

  pkgconf_client_set_flags (c, client_flags);

  /* Bootstrap the package search default paths if not specified explicitly.
   */
  if (default_dirs)
    pkgconf_client_dir_list_build (c);

  pkgconf_pkg_t* p = pkgconf_pkg_find (c, name);

  if (p != NULL)
  {
    unsigned int e = LIBPKG_CONFIG_PKG_ERRF_OK;

    /* Print C flags.
     */
    if (cflags)
    {
      pkgconf_client_set_flags (
        c,
        client_flags | LIBPKG_CONFIG_PKG_PKGF_SEARCH_PRIVATE);

      pkgconf_list_t list = LIBPKG_CONFIG_LIST_INITIALIZER;
      e = pkgconf_pkg_cflags (c, p, &list, max_depth);

      if (e == LIBPKG_CONFIG_PKG_ERRF_OK)
        print_and_free (&list);

      pkgconf_client_set_flags (c, client_flags); /* Restore. */
    }

    /* Print libs.
     */
    if (libs && e == LIBPKG_CONFIG_PKG_ERRF_OK)
    {
      pkgconf_list_t list = LIBPKG_CONFIG_LIST_INITIALIZER;
      e = pkgconf_pkg_libs (c, p, &list, max_depth);

      if (e == LIBPKG_CONFIG_PKG_ERRF_OK)
        print_and_free (&list);
    }

    if (e == LIBPKG_CONFIG_PKG_ERRF_OK)
    {
      r = 0;

      if (cflags || libs)
        printf ("\n");
    }

    pkgconf_pkg_unref (c, p);
  }
  else
    fprintf (stderr, "package '%s' not found\n", name);

  pkgconf_client_free (c);
  return r;
}
