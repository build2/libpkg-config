/*
 * path.c
 * filesystem path management
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

#include <libpkg-config/config.h>
#include <libpkg-config/stdinc.h>
#include <libpkg-config/bsdstubs.h>

#if defined(HAVE_CYGWIN_CONV_PATH) && defined(__MSYS__)
# include <sys/cygwin.h>
#endif

#if defined(HAVE_SYS_STAT_H) && ! defined(_WIN32)
# include <sys/stat.h>
# define PKGCONF_CACHE_INODES
#endif

static bool
#ifdef PKGCONF_CACHE_INODES
path_list_contains_entry(const char *text, pkgconf_list_t *dirlist, struct stat *st)
#else
path_list_contains_entry(const char *text, pkgconf_list_t *dirlist)
#endif
{
	pkgconf_node_t *n;

	LIBPKG_CONFIG_FOREACH_LIST_ENTRY(dirlist->head, n)
	{
		pkgconf_path_t *pn = n->data;

#ifdef PKGCONF_CACHE_INODES
		if (pn->handle_device == (void *)(intptr_t)st->st_dev && pn->handle_path == (void *)(intptr_t)st->st_ino)
			return true;
#endif

		if (!strcmp(text, pn->path))
			return true;
	}

	return false;
}

/*
 * !doc
 *
 * libpkg-config `path` module
 * ===========================
 *
 * The `path` module provides functions for manipulating lists of paths in a cross-platform manner.  Notably,
 * it is used by the `pkgconf client` to parse the ``PKG_CONFIG_PATH``, ``PKG_CONFIG_LIBDIR`` and related environment
 * variables.
 */

/*
 * !doc
 *
 * .. c:function:: void pkgconf_path_add(const char *text, pkgconf_list_t *dirlist)
 *
 *    Adds a path node to a path list.  If the path is already in the list, do nothing.
 *
 *    :param char* text: The path text to add as a path node.
 *    :param pkgconf_list_t* dirlist: The path list to add the path node to.
 *    :param bool filter: Whether to perform duplicate filtering.
 *    :return: nothing
 */
void
pkgconf_path_add(const char *text, pkgconf_list_t *dirlist, bool filter)
{
	pkgconf_path_t *node;
	char path[PKGCONF_ITEM_SIZE];

	pkgconf_strlcpy(path, text, sizeof path);
	pkgconf_path_relocate(path, sizeof path);

#ifdef PKGCONF_CACHE_INODES
	struct stat st;

	if (filter)
	{
		if (lstat(path, &st) == -1)
			return;
		if (S_ISLNK(st.st_mode))
		{
			char *linkdest = realpath(path, NULL);

			if (linkdest != NULL && stat(linkdest, &st) == -1)
			{
				free(linkdest);
				return;
			}

			free(linkdest);
		}
		if (path_list_contains_entry(path, dirlist, &st))
			return;
	}
#else
	if (filter && path_list_contains_entry(path, dirlist))
		return;
#endif

	node = calloc(sizeof(pkgconf_path_t), 1);
	node->path = strdup(path);

#ifdef PKGCONF_CACHE_INODES
	if (filter) {
		node->handle_path = (void *)(intptr_t) st.st_ino;
		node->handle_device = (void *)(intptr_t) st.st_dev;
	}
#endif

	pkgconf_node_insert_tail(&node->lnode, node, dirlist);
}

/*
 * !doc
 *
 * .. c:function:: size_t pkgconf_path_split(const char *text, pkgconf_list_t *dirlist)
 *
 *    Splits a given text input and inserts paths into a path list.
 *
 *    :param char* text: The path text to split and add as path nodes.
 *    :param pkgconf_list_t* dirlist: The path list to have the path nodes added to.
 *    :param bool filter: Whether to perform duplicate filtering.
 *    :return: number of path nodes added to the path list
 *    :rtype: size_t
 */
size_t
pkgconf_path_split(const char *text, pkgconf_list_t *dirlist, bool filter)
{
	size_t count = 0;
	char *workbuf, *p, *iter;

	if (text == NULL)
		return 0;

	iter = workbuf = strdup(text);
	while ((p = strtok(iter, LIBPKG_CONFIG_PATH_SEP_S)) != NULL)
	{
		pkgconf_path_add(p, dirlist, filter);

		count++, iter = NULL;
	}
	free(workbuf);

	return count;
}

/*
 * !doc
 *
 * .. c:function:: size_t pkgconf_path_build_from_environ(const char *envvarname, const char *fallback, pkgconf_list_t *dirlist)
 *
 *    Adds the paths specified in an environment variable to a path list.  If the environment variable is not set,
 *    an optional default set of paths is added.
 *
 *    :param char* envvarname: The environment variable to look up.
 *    :param char* fallback: The fallback paths to use if the environment variable is not set.
 *    :param pkgconf_list_t* dirlist: The path list to add the path nodes to.
 *    :param bool filter: Whether to perform duplicate filtering.
 *    :return: number of path nodes added to the path list
 *    :rtype: size_t
 */
size_t
pkgconf_path_build_from_environ(const char *envvarname, const char *fallback, pkgconf_list_t *dirlist, bool filter)
{
	const char *data;

	data = getenv(envvarname);
	if (data != NULL)
		return pkgconf_path_split(data, dirlist, filter);

	if (fallback != NULL)
		return pkgconf_path_split(fallback, dirlist, filter);

	/* no fallback and no environment variable, thusly no nodes added */
	return 0;
}

/*
 * !doc
 *
 * .. c:function:: bool pkgconf_path_match_list(const char *path, const pkgconf_list_t *dirlist)
 *
 *    Checks whether a path has a matching prefix in a path list.
 *
 *    :param char* path: The path to check against a path list.
 *    :param pkgconf_list_t* dirlist: The path list to check the path against.
 *    :return: true if the path list has a matching prefix, otherwise false
 *    :rtype: bool
 */
bool
pkgconf_path_match_list(const char *path, const pkgconf_list_t *dirlist)
{
	pkgconf_node_t *n = NULL;
	char relocated[PKGCONF_ITEM_SIZE];
	const char *cpath = path;

	pkgconf_strlcpy(relocated, path, sizeof relocated);
	if (pkgconf_path_relocate(relocated, sizeof relocated))
		cpath = relocated;

	LIBPKG_CONFIG_FOREACH_LIST_ENTRY(dirlist->head, n)
	{
		pkgconf_path_t *pnode = n->data;

		if (!strcmp(pnode->path, cpath))
			return true;
	}

	return false;
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_path_copy_list(pkgconf_list_t *dst, const pkgconf_list_t *src)
 *
 *    Copies a path list to another path list.
 *
 *    :param pkgconf_list_t* dst: The path list to copy to.
 *    :param pkgconf_list_t* src: The path list to copy from.
 *    :return: nothing
 */
void
pkgconf_path_copy_list(pkgconf_list_t *dst, const pkgconf_list_t *src)
{
	pkgconf_node_t *n;

	LIBPKG_CONFIG_FOREACH_LIST_ENTRY(src->head, n)
	{
		pkgconf_path_t *srcpath = n->data, *path;

		path = calloc(sizeof(pkgconf_path_t), 1);
		path->path = strdup(srcpath->path);

#ifdef PKGCONF_CACHE_INODES
		path->handle_path = srcpath->handle_path;
		path->handle_device = srcpath->handle_device;
#endif

		pkgconf_node_insert_tail(&path->lnode, path, dst);
	}
}

/*
 * !doc
 *
 * .. c:function:: void pkgconf_path_free(pkgconf_list_t *dirlist)
 *
 *    Releases any path nodes attached to the given path list.
 *
 *    :param pkgconf_list_t* dirlist: The path list to clean up.
 *    :return: nothing
 */
void
pkgconf_path_free(pkgconf_list_t *dirlist)
{
	pkgconf_node_t *n, *tn;

	LIBPKG_CONFIG_FOREACH_LIST_ENTRY_SAFE(dirlist->head, tn, n)
	{
		pkgconf_path_t *pnode = n->data;

		free(pnode->path);
		free(pnode);
	}
}

static char *
normpath(const char *path)
{
	if (!path)
		return NULL;

	char *copy = strdup(path);
	if (NULL == copy)
		return NULL;
	char *ptr = copy;

	for (int ii = 0; copy[ii]; ii++)
	{
		*ptr++ = path[ii];
		if ('/' == path[ii])
		{
			ii++;
			while ('/' == path[ii])
				ii++;
			ii--;
		}
	}
	*ptr = '\0';

	return copy;
}

/*
 * !doc
 *
 * .. c:function:: bool pkgconf_path_relocate(char *buf, size_t buflen)
 *
 *    Relocates a path, possibly calling normpath() or cygwin_conv_path() on it.
 *
 *    :param char* buf: The path to relocate.
 *    :param size_t buflen: The buffer length the path is contained in.
 *    :return: true on success, false on error
 *    :rtype: bool
 */
bool
pkgconf_path_relocate(char *buf, size_t buflen)
{
#if defined(HAVE_CYGWIN_CONV_PATH) && defined(__MSYS__)
	ssize_t size;
	char *tmpbuf, *ti;

	size = cygwin_conv_path(CCP_POSIX_TO_WIN_A, buf, NULL, 0);
	if (size < 0 || (size_t) size > buflen)
		return false;

	tmpbuf = malloc(size);
	if (cygwin_conv_path(CCP_POSIX_TO_WIN_A, buf, tmpbuf, size))
		return false;

	pkgconf_strlcpy(buf, tmpbuf, buflen);
	free(tmpbuf);

	/* rewrite any backslash arguments for best compatibility */
	for (ti = buf; *ti != '\0'; ti++)
	{
		if (*ti == '\\')
			*ti = '/';
	}
#else
	char *tmpbuf;

	if ((tmpbuf = normpath(buf)) != NULL)
	{
		size_t tmpbuflen = strlen(tmpbuf);
		if (tmpbuflen > buflen)
		{
			free(tmpbuf);
			return false;
		}

		pkgconf_strlcpy(buf, tmpbuf, buflen);
		free(tmpbuf);
	}
#endif

	return true;
}