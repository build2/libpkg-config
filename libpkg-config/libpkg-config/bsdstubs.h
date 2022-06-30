/*
 * bsdstubs.h
 * Header for stub BSD function prototypes if unavailable on a specific platform.
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

#ifndef LIBPKGCONF_BSDSTUBS_H
#define LIBPKGCONF_BSDSTUBS_H

#include <libpkg-config/libpkgconf-api.h>

#ifdef __cplusplus
extern "C" {
#endif

PKGCONF_API extern size_t pkgconf_strlcpy(char *dst, const char *src, size_t siz);
PKGCONF_API extern size_t pkgconf_strlcat(char *dst, const char *src, size_t siz);
PKGCONF_API extern char *pkgconf_strndup(const char *src, size_t len);

#ifdef __cplusplus
}
#endif

#endif
