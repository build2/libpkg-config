/*
 * fileio.c
 * File reading utilities
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

char*
pkg_config_fgetline (char* line, size_t size, FILE* stream)
{
  char* s = line;
  char* end = line + size - 2; /* Potentially assign 2 char in loop. */
  bool quoted = false;
  int c = '\0', c2;

  if (s == NULL)
    return NULL;

  while (s < end && (c = getc (stream)) != EOF)
  {
    if (c == '\\' && !quoted)
    {
      quoted = true;
      continue;
    }
    else if (c == '#')
    {
      if (!quoted)
      {
        /* Skip the rest of the line */
        do
        {
          c = getc (stream);
        } while (c != '\n' && c != EOF);
        *s++ = '\n';
        break;
      }
      else
        *s++ = c;

      quoted = false;
      continue;
    }
    else if (c == '\n')
    {
      if (quoted)
      {
        /* Trim spaces */
        do
        {
          c2 = getc (stream);
        } while (c2 == '\t' || c2 == ' ');

        ungetc (c2, stream);

        quoted = false;
        continue;
      }
      else
        *s++ = c;

      break;
    }
    else if (c == '\r')
    {
      *s++ = '\n';

      if ((c2 = getc (stream)) == '\n')
      {
        if (quoted)
        {
          quoted = false;
          continue;
        }

        break;
      }

      ungetc (c2, stream);

      if (quoted)
      {
        quoted = false;
        continue;
      }

      break;
    }
    else
    {
      if (quoted)
      {
        *s++ = '\\';
        quoted = false;
      }
      *s++ = c;
    }
  }

  if (c == EOF && (s == line || ferror (stream)))
    return NULL;

  *s = '\0';

  /* Remove newline character. */
  if (s > line && *(--s) == '\n')
  {
    *s = '\0';

    if (s > line && *(--s) == '\r')
      *s = '\0';
  }

  return line;
}
