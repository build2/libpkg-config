/*
 * argvsplit.c
 * argv_split() routine
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
 * libpkg-config `argvsplit` module
 * ================================
 *
 * This is a lowlevel module which provides parsing of strings into argument
 * vectors, similar to what a shell would do.
 */

/*
 * !doc
 *
 * .. c:function:: void pkg_config_argv_free(char **argv)
 *
 *    Frees an argument vector.
 *
 *    :param char** argv: The argument vector to free.
 *    :return: nothing
 */
void
pkg_config_argv_free (char** argv)
{
  free (argv[0]);
  free (argv);
}

/*
 * !doc
 *
 * .. c:function:: int pkg_config_argv_split(const char *src, int *argc, char
 * ***argv)
 *
 *    Splits a string into an argument vector.
 *
 *    :param char*   src: The string to split.
 *    :param int*    argc: A pointer to an integer to store the argument
 * count. :param char*** argv: A pointer to a pointer for an argument vector.
 *    :return: 0 on success, -1 on error.
 *    :rtype: int
 */
int
pkg_config_argv_split (const char* src, int* argc, char*** argv)
{
  char* buf = malloc (strlen (src) + 1);
  const char* src_iter;
  char* dst_iter;
  int argc_count = 0;
  int argv_size = 5;
  char quote = 0;
  bool escaped = false;

  src_iter = src;
  dst_iter = buf;

  memset (buf, 0, strlen (src) + 1);

  *argv = calloc (argv_size, sizeof (void*));
  (*argv)[argc_count] = dst_iter;

  while (*src_iter)
  {
    if (escaped)
    {
      /* POSIX: only \CHAR is special inside a double quote if CHAR is {$, `,
       * ", \, newline}. */
      if (quote == '\"')
      {
        if (!(*src_iter == '$' || *src_iter == '`' || *src_iter == '"' ||
              *src_iter == '\\'))
          *dst_iter++ = '\\';

        *dst_iter++ = *src_iter;
      }
      else
        *dst_iter++ = *src_iter;

      escaped = false;
    }
    else if (quote)
    {
      if (*src_iter == quote)
        quote = 0;
      else if (*src_iter == '\\' && quote != '\'')
        escaped = true;
      else
        *dst_iter++ = *src_iter;
    }
    else if (isspace ((unsigned int)*src_iter))
    {
      if ((*argv)[argc_count] != NULL)
      {
        argc_count++, dst_iter++;

        if (argc_count == argv_size)
        {
          argv_size += 5;
          *argv = realloc (*argv, sizeof (void*) * argv_size);
        }

        (*argv)[argc_count] = dst_iter;
      }
    }
    else
      switch (*src_iter)
      {
      case '\\':
        escaped = true;
        break;

      case '\"':
      case '\'':
        quote = *src_iter;
        break;

      default:
        *dst_iter++ = *src_iter;
        break;
      }

    src_iter++;
  }

  if (escaped || quote)
  {
    free (*argv);
    free (buf);
    return -1;
  }

  if (strlen ((*argv)[argc_count]))
  {
    argc_count++;
  }

  *argc = argc_count;
  return 0;
}
