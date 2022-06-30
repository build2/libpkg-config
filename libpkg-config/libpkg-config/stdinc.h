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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <stdint.h>

#define PKGCONF_BUFSIZE	(65535)

#define PKGCONF_ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))

#ifndef PKGCONF_LITE
#if defined(__GNUC__) || defined(__INTEL_COMPILER)
#define PKGCONF_TRACE(client, ...) do { \
		pkgconf_trace(client, __FILE__, __LINE__, __PRETTY_FUNCTION__, __VA_ARGS__); \
	} while (0);
#else
#define PKGCONF_TRACE(client, ...) do { \
		pkgconf_trace(client, __FILE__, __LINE__, __func__, __VA_ARGS__); \
	} while (0);
#endif
#else
#define PKGCONF_TRACE(client, ...)
#endif

#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <malloc.h>
# define PATH_DEV_NULL	"nul"

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
# ifndef __MINGW32__
#  include "win-dirent.h"
# else
# include <dirent.h>
# endif
# define PKGCONF_ITEM_SIZE (_MAX_PATH + 1024)

#else /* _WIN32 */

# define PATH_DEV_NULL	"/dev/null"
# define SIZE_FMT_SPECIFIER	"%zu"
# ifdef __HAIKU__
#  include <FindDirectory.h>
# endif
# include <dirent.h>
# include <unistd.h>
# include <limits.h>
# ifdef PATH_MAX
#  define PKGCONF_ITEM_SIZE (PATH_MAX + 1024)
# else
#  define PKGCONF_ITEM_SIZE (4096 + 1024)
# endif
#endif

#endif /* LIBPKG_CONFIG_STDINC_H */