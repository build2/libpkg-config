/*
 * parser.c
 * rfc822 message parser
 *
 * ISC License
 *
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
 * .. c:function:: pkgconf_pkg_t *pkgconf_pkg_new_from_file(const
 * pkgconf_client_t *client, const char *filename, FILE *f)
 *
 *    Parse a .pc file into a pkgconf_pkg_t object structure.
 *
 *    :param pkgconf_client_t* client: The pkgconf client object to use for
 * dependency resolution. :param char* filename: The filename of the package
 * file (including full path). :param FILE* f: The file object to read from.
 *    :returns: A ``pkgconf_pkg_t`` object which contains the package data.
 *    :rtype: pkgconf_pkg_t *
 */
void
pkgconf_parser_parse (FILE* f,
                      void* data,
                      const pkgconf_parser_operand_func_t* ops,
                      const pkgconf_parser_warn_func_t warnfunc,
                      const char* filename)
{
  char readbuf[PKGCONF_BUFSIZE];
  size_t lineno = 0;

  while (pkgconf_fgetline (readbuf, PKGCONF_BUFSIZE, f) != NULL)
  {
    char op, *p, *key, *value;
    bool warned_key_whitespace = false, warned_value_whitespace = false;

    lineno++;

    p = readbuf;
    while (*p && (isalpha ((unsigned int)*p) || isdigit ((unsigned int)*p) ||
                  *p == '_' || *p == '.'))
      p++;

    key = readbuf;
    if (!isalpha ((unsigned int)*key) && !isdigit ((unsigned int)*p))
      continue;

    while (*p && isspace ((unsigned int)*p))
    {
      if (!warned_key_whitespace)
      {
        warnfunc (
            data,
            "%s:" SIZE_FMT_SPECIFIER
            ": warning: whitespace encountered while parsing key section",
            filename,
            lineno);
        warned_key_whitespace = true;
      }

      /* set to null to avoid trailing spaces in key */
      *p = '\0';
      p++;
    }

    op = *p;
    if (*p != '\0') /* Increment already done in above loop? */
    {
      *p = '\0';
      p++;
    }

    while (*p && isspace ((unsigned int)*p)) p++;

    value = p;
    p = value + (strlen (value) - 1);
    while (*p && isspace ((unsigned int)*p) && p > value)
    {
      if (!warned_value_whitespace && op == '=')
      {
        warnfunc (data,
                  "%s:" SIZE_FMT_SPECIFIER
                  ": warning: trailing whitespace encountered while parsing "
                  "value section",
                  filename,
                  lineno);
        warned_value_whitespace = true;
      }

      *p = '\0';
      p--;
    }

    if (ops[(unsigned char)op])
      ops[(unsigned char)op](data, lineno, key, value);
  }

  fclose (f);
}
