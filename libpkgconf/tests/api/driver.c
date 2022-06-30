/* file      : tests/api/driver.c
 * license   : ISC; see accompanying COPYING file
 */

/* Enable assertions.
 */
#ifdef NDEBUG
#  undef NDEBUG
#endif

#include <libpkgconf/libpkgconf.h>

#include <stdio.h>   /* printf(), fprintf(), stderr */
#include <stddef.h>  /* size_t, NULL */
#include <assert.h>
#include <string.h>  /* strcmp(), strlen() */
#include <stdbool.h> /* bool, true, false */

static bool
error_handler (const char* msg, const pkgconf_client_t* c, const void* d)
{
  (void) c; /* Unused. */
  (void) d; /* Unused. */

  /* Seems it always have a trailing newline char. Probably it still a good
   * idea to check if it is. Let's see if it ever be missed.
   */
  fprintf (stderr, "%s", msg);
  return true;
}

static void
frags_print_and_free (pkgconf_list_t* list)
{
  pkgconf_node_t *node;
  PKGCONF_FOREACH_LIST_ENTRY(list->head, node)
  {
    pkgconf_fragment_t* frag = node->data;
    printf("%c %s\n", frag->type != '\0' ? frag->type : ' ', frag->data);
  }

  pkgconf_fragment_free (list);
}

static void
tuples_print (pkgconf_list_t *list)
{
  pkgconf_node_t *node;
  PKGCONF_FOREACH_LIST_ENTRY(list->head, node)
  {
    pkgconf_tuple_t *tuple = node->data;

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

  pkgconf_client_t* c =
    pkgconf_client_new (error_handler,
                        NULL /* error_handler_data */,
                        pkgconf_cross_personality_default ());

  assert (c != NULL);

  int r = 1;
  int max_depth = 2000;

  const int pkgconf_flags = PKGCONF_PKG_PKGF_DONT_MERGE_SPECIAL_FRAGMENTS;

  pkgconf_client_set_flags (c, pkgconf_flags);
  pkgconf_pkg_t* p = pkgconf_pkg_find (c, path);

  if (p != NULL)
  {
    int e = PKGCONF_PKG_ERRF_OK;

    switch (mode)
    {
    case dump_cflags:
      {
        pkgconf_client_set_flags (c,
                                  pkgconf_flags |
                                  PKGCONF_PKG_PKGF_SEARCH_PRIVATE);

        pkgconf_list_t list = PKGCONF_LIST_INITIALIZER;
        e = pkgconf_pkg_cflags (c, p, &list, max_depth);

        if (e == PKGCONF_PKG_ERRF_OK)
          frags_print_and_free (&list);

        pkgconf_client_set_flags (c, 0); /* Restore. */
        break;
      }
    case dump_libs:
      {
        pkgconf_client_set_flags (c,
                                  pkgconf_flags                   |
                                  PKGCONF_PKG_PKGF_SEARCH_PRIVATE |
                                  PKGCONF_PKG_PKGF_MERGE_PRIVATE_FRAGMENTS);

        pkgconf_list_t list = PKGCONF_LIST_INITIALIZER;
        e = pkgconf_pkg_libs (c, p, &list, max_depth);

        if (e == PKGCONF_PKG_ERRF_OK)
          frags_print_and_free (&list);

        pkgconf_client_set_flags (c, 0); /* Restore. */
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

    if (e == PKGCONF_PKG_ERRF_OK)
      r = 0;

    pkgconf_pkg_unref (c, p);
  }
  else
    fprintf (stderr, "package file '%s' not found or invalid\n", path);

  pkgconf_client_free (c);
  return r;
}
