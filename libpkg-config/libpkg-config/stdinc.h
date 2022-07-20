/*
 * stdinc.h
 * Pull in standard headers (including portability hacks). Plus internal
 * constants, etc.
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

#ifndef LIBPKG_CONFIG_STDINC_H
#define LIBPKG_CONFIG_STDINC_H

#include <libpkg-config/config.h>

#include <ctype.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN
# include <windows.h> /* @@ I wonder if we still need it? */
# include <malloc.h>
# define PATH_DEV_NULL "nul"

# ifndef ssize_t
#  ifndef __MINGW32__
#   include <BaseTsd.h>
#  else
#   include <basetsd.h>
#  endif
#  define ssize_t SSIZE_T
# endif
# define PKG_CONFIG_ITEM_SIZE (_MAX_PATH + 1024)

# define strncasecmp _strnicmp
# define strcasecmp _stricmp

#else /* _WIN32 */

# define PATH_DEV_NULL "/dev/null"
# include <unistd.h>
# include <limits.h>
# ifdef PATH_MAX
#  define PKG_CONFIG_ITEM_SIZE (PATH_MAX + 1024)
# else
#  define PKG_CONFIG_ITEM_SIZE (4096 + 1024)
# endif
#endif

/* Besides other things, this size limits the length of the line that we can
 * read from a .pc file and thus the size of variable name/value.
 */
#define PKG_CONFIG_BUFSIZE (65535)

#define PKG_CONFIG_ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))

/* Hook NTRACE to NDEBUG. This both feels natural and gives us a way to
 * test this mode (since we have NDEBUG configurations on CI).
 */
#if !defined(LIBPKG_CONFIG_NTRACE) && defined(NDEBUG)
#  define LIBPKG_CONFIG_NTRACE
#endif

#ifndef LIBPKG_CONFIG_NTRACE
#if defined(__GNUC__) || defined(__INTEL_COMPILER)
#define PKG_CONFIG_TRACE(client, ...) do { \
    pkg_config_trace(client, __FILE__, __LINE__, __PRETTY_FUNCTION__, __VA_ARGS__); \
  } while (0)
#else
#define PKG_CONFIG_TRACE(client, ...) do { \
    pkg_config_trace(client, __FILE__, __LINE__, __func__, __VA_ARGS__);   \
  } while (0)
#endif
#else
#define PKG_CONFIG_TRACE(client, ...) do {} while (0)
#endif

#ifdef _WIN32
#  undef PKG_CONFIG_DEFAULT_PATH
#  define PKG_CONFIG_DEFAULT_PATH "../lib/pkgconfig;../share/pkgconfig"
#endif

#define PKG_CONFIG_EXT ".pc"

extern size_t pkg_config_strlcpy(char *dst, const char *src, size_t siz);
extern size_t pkg_config_strlcat(char *dst, const char *src, size_t siz);
extern char *pkg_config_strndup(const char *src, size_t len);

#endif /* LIBPKG_CONFIG_STDINC_H */
