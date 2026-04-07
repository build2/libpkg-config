/*
 * parser.c
 * rfc822 message parser
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
 * .. c:function:: pkg_config_pkg_t *pkg_config_parser_parse(const
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
unsigned int
pkg_config_parser_parse (pkg_config_client_t* client,
                         FILE* f,
                         void* data,
                         const pkg_config_parser_operand_func_t* ops,
                         size_t ops_count,
                         const char* filename)
{
  unsigned int eflags = LIBPKG_CONFIG_ERRF_OK;

  size_t lineno = 0;

  /* Determine the file size and allocate a buffer of that size (plus 2; see
     pkg_config_fgetline()) to avoid truncations (which this code does not
     handle gracefully). */
  size_t readbufn;
  char* readbuf;
  {
    /* While we are likely at the beginning of the stream, let's not rely on
     * that. Instead of the file size, let's calculate the number of unread
     * characters in the stream. That will also be more accurate in regards to
     * the translation of the character sequences in the text mode (\r\n,
     * etc).
     *
     * Note that positioning to the end of the stream, by calling fseek(), and
     * then trying to interpret the value returned by ftell() as a number of
     * characters is generally not a good idea in the text mode (which we are
     * in). The latter is actually discouraged by the standard and the safe
     * way to calculate the number of characters in the stream would be
     * calling fread() until EOF is encountered and rewinding to the initial
     * position afterwards. However, this approach may potentially result in a
     * performance penalty for projects with big .pc files and/or big number
     * of dependencies. Thus, let's still use the fseek()/ftell() based
     * approach, assuming that in the text mode for all the major
     * implementations ftell() returns a number, which is always greater or
     * equal to the number of characters present in the stream before the
     * current reading position. The only drawback (besides not being
     * standard-compliant) is that we may allocate a bit larger buffer, than
     * it is required for the parsing.
     *
     * Also note that we used to call fileno() and fstat() to retrieve the
     * file size. However, the information returned by fstat() seems to not
     * always be accurate on FreeBSD, probably due to the file
     * meta-information synchronization delay.
     */
    long bo = ftell (f);

    /* A non-zero value denotes the calculated buffer size, including 2
     * additional characters (see above). The zero value indicates that an
     * error has occurred.
     */
    readbufn = 0;

    long eo; // Offset of EOF.
    if (bo >= 0                     &&
        fseek (f, 0, SEEK_END) == 0 &&
        (eo = ftell (f)) >= 0       &&
        fseek (f, bo, SEEK_SET) == 0)
    {
      assert (eo >= bo);

#if 1
      readbufn += (size_t)(eo - bo) + 2;
#else
      for (;;)
      {
        char buf[1024];
        size_t n = fread (buf, 1, sizeof (buf), f);

        if (n != sizeof (buf))
        {
          if (ferror (f) == 0 && fseek (f, bo, SEEK_SET) == 0)
          {
            readbufn += n;

            // Verify the above assumption.
            //
            assert ((size_t)(eo - bo) >= readbufn);

            readbufn += 2; // Reserve for extra characters (see above).
          }
          else
            readbufn = 0; // Error occured.

          break;
        }
        else
          readbufn += n;
      }
#endif
    }

    if (readbufn == 0)
    {
      eflags = LIBPKG_CONFIG_ERRF_PACKAGE_INVALID;
      pkg_config_error (client,
                        eflags,
                        filename,
                        lineno,
                        "unable to read file");

      fclose (f);
      return eflags;
    }

    readbuf = malloc (readbufn);
  }

  while (pkg_config_fgetline (readbuf, readbufn, f) != NULL)
  {
    char op, *p, *key, *value;
#if 0
    bool warned_key_whitespace = false;
#endif
    bool warned_value_whitespace = false;

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
      /* It seems silly to warn about trailing whitespaces in keys. For
         example, this warns if we put a whitespace before `=` in a variable
         assignment, as in `var = value`. */
#if 0
      if (!warned_key_whitespace)
      {
        pkg_config_warn (
          client,
          filename,
          lineno,
          "whitespace encountered while parsing key section");
        warned_key_whitespace = true;
      }
#endif

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
    if (*value != '\0')
    {
      p = value + (strlen (value) - 1);
      while (p >= value && isspace ((unsigned int)*p))
      {
        if (!warned_value_whitespace && op == '=')
        {
          pkg_config_warn (
            client,
            filename,
            lineno,
            "trailing whitespace encountered while parsing value section");

          warned_value_whitespace = true;
        }

        *p = '\0';
        p--;
      }
    }

    unsigned char i = (unsigned char)op;

    if (i >= ops_count || ops[i] == NULL)
    {
      eflags = LIBPKG_CONFIG_ERRF_FILE_INVALID_SYNTAX;
      pkg_config_error (client,
                        eflags,
                        filename,
                        lineno,
                        "unexpected key/value separator '%c'",
                        op);
      break;
    }
    else
    {
      eflags = ops[i](data, lineno, key, value);
      if (eflags != LIBPKG_CONFIG_ERRF_OK)
        break;
    }
  }

  free (readbuf);
  fclose (f);
  return eflags;
}
