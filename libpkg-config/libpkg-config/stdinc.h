/*
 * stdinc.h
 * Pull in standard headers (including portability hacks). Plus internal
 * constants, etc.
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

#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN
# include <windows.h> /* @@ I wonder if we still need it? */
# include <malloc.h>
# define PATH_DEV_NULL	"nul"

# define strncasecmp _strnicmp
# define strcasecmp _stricmp

/* Note that MinGW's printf() format semantics have changed starting GCC 10.
 * In particular, GCC 10 complains about MSVC's 'I64' length modifier but now
 * accepts the standard (C99) 'z' modifier.
 */
# if defined(__GNUC__) && __GNUC__ >= 10
#  define SIZE_FMT_SPECIFIER	"%zu"
# else
#  ifdef _WIN64
#   define SIZE_FMT_SPECIFIER	"%I64u"
#  else
#   define SIZE_FMT_SPECIFIER	"%u"
#  endif
# endif

# ifndef ssize_t
# ifndef __MINGW32__
#  include <BaseTsd.h>
# else
#  include <basetsd.h>
# endif
#  define ssize_t SSIZE_T
# endif
# define PKGCONF_ITEM_SIZE (_MAX_PATH + 1024)

#else /* _WIN32 */

# define PATH_DEV_NULL	"/dev/null"
# define SIZE_FMT_SPECIFIER	"%zu"
# include <unistd.h>
# include <limits.h>
# ifdef PATH_MAX
#  define PKGCONF_ITEM_SIZE (PATH_MAX + 1024)
# else
#  define PKGCONF_ITEM_SIZE (4096 + 1024)
# endif
#endif

#define PKGCONF_BUFSIZE	(65535)

#define PKGCONF_ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))

/* Hook NTRACE to NDEBUG. This both feels natural and gives us a way to
 * test this mode (since we have NDEBUG configurations on CI).
 */
#if !defined(LIBPKG_CONFIG_NTRACE) && defined(NDEBUG)
#  define LIBPKG_CONFIG_NTRACE
#endif

#ifndef LIBPKG_CONFIG_NTRACE
#if defined(__GNUC__) || defined(__INTEL_COMPILER)
#define PKGCONF_TRACE(client, ...) do { \
		pkgconf_trace(client, __FILE__, __LINE__, __PRETTY_FUNCTION__, __VA_ARGS__); \
	} while (0)
#else
#define PKGCONF_TRACE(client, ...) do { \
		pkgconf_trace(client, __FILE__, __LINE__, __func__, __VA_ARGS__); \
	} while (0)
#endif
#else
#define PKGCONF_TRACE(client, ...) do {} while (0)
#endif

#ifdef _WIN32
#	undef PKG_DEFAULT_PATH
#	define PKG_DEFAULT_PATH "../lib/pkgconfig;../share/pkgconfig"
#endif

#define PKG_CONFIG_EXT ".pc"

extern size_t pkgconf_strlcpy(char *dst, const char *src, size_t siz);
extern size_t pkgconf_strlcat(char *dst, const char *src, size_t siz);
extern char *pkgconf_strndup(const char *src, size_t len);

#endif /* LIBPKG_CONFIG_STDINC_H */
