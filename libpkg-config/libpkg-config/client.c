/*
 * client.c
 * Consumer lifecycle management.
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
 * libpkg-config `client` module
 * =============================
 *
 * The libpkg-config `client` module implements the `pkg_config_client_t`
 * "client" object. Client objects store all necessary state for libpkg-config
 * allowing for multiple instances to run in parallel.
 *
 * Client objects are not thread safe, in other words, a client object should
 * not be shared across thread boundaries.
 */

#ifndef LIBPKG_CONFIG_NTRACE
static void
trace_path_list (const pkg_config_client_t* client,
                 const char* desc,
                 pkg_config_list_t* list)
{
  const pkg_config_node_t* n;

  PKG_CONFIG_TRACE (client, "%s:", desc);
  LIBPKG_CONFIG_FOREACH_LIST_ENTRY (list->head, n)
  {
    const pkg_config_path_t* p = n->data;

    PKG_CONFIG_TRACE (client, "  - '%s'", p->path);
  }
}
#endif

static inline void
build_default_search_path (pkg_config_list_t* dirlist)
{
#ifdef _WIN32
  char namebuf[MAX_PATH];
  char outbuf[MAX_PATH];
  char* p;

  int sizepath = GetModuleFileName (NULL, namebuf, sizeof namebuf);
  char* winslash;
  namebuf[sizepath] = '\0';
  while ((winslash = strchr (namebuf, '\\')) != NULL)
  {
    *winslash = '/';
  }
  p = strrchr (namebuf, '/');
  if (p == NULL)
    pkg_config_path_split (PKG_CONFIG_DEFAULT_PATH, dirlist, true);

  *p = '\0';
  pkg_config_strlcpy (outbuf, namebuf, sizeof outbuf);
  pkg_config_strlcat (outbuf, "/", sizeof outbuf);
  pkg_config_strlcat (outbuf, "../lib/pkgconfig", sizeof outbuf);
  pkg_config_path_add (outbuf, dirlist, true);
  pkg_config_strlcpy (outbuf, namebuf, sizeof outbuf);
  pkg_config_strlcat (outbuf, "/", sizeof outbuf);
  pkg_config_strlcat (outbuf, "../share/pkgconfig", sizeof outbuf);
  pkg_config_path_add (outbuf, dirlist, true);
#else
  pkg_config_path_split (PKG_CONFIG_DEFAULT_PATH, dirlist, true);
#endif
}

/*
 * !doc
 *
 * .. c:function:: void pkg_config_client_dir_list_build(pkg_config_client_t
 * *client)
 *
 *    Bootstraps the package search paths.  If the
 *    ``LIBPKG_CONFIG_PKG_PKGF_ENV_ONLY`` `flag` is set on the client, then
 *    only the ``PKG_CONFIG_PATH`` and ``PKG_CONFIG_LIBDIR`` environment
 *    variables will be used, otherwise both the environment variables and
 *    the default will be considered.
 *
 *    :param pkg_config_client_t* client: The pkg-config client object to
 * bootstrap. :return: nothing
 */
void
pkg_config_client_dir_list_build (pkg_config_client_t* client)
{
  pkg_config_path_build_from_environ (
      "PKG_CONFIG_PATH", NULL, &client->dir_list, true);

  if (getenv ("PKG_CONFIG_LIBDIR") != NULL)
  {
    /* PKG_CONFIG_LIBDIR= should empty the default search path entirely. */
    pkg_config_path_build_from_environ (
        "PKG_CONFIG_LIBDIR", NULL, &client->dir_list, true);
  }
  else if (!(client->flags & LIBPKG_CONFIG_PKG_PKGF_ENV_ONLY))
    build_default_search_path (&client->dir_list);
}

/*
 * !doc
 *
 * .. c:function:: void pkg_config_client_init(pkg_config_client_t *client,
 * pkg_config_error_handler_func_t error_handler, void *error_handler_data,
 * bool init_filters)
 *
 *    Initialise a pkg-config client object.
 *
 *    :param pkg_config_client_t* client: The client to initialise.
 *    :param pkg_config_error_handler_func_t error_handler: An optional error
 * handler to use for logging errors. :param void* error_handler_data: user
 * data passed to optional error handler :param bool init_filters: whether two
 * initialize include/lib path filters from environment/defaults :return:
 * nothing
 */
void
pkg_config_client_init (pkg_config_client_t* client,
                        pkg_config_error_handler_func_t error_handler,
                        void* error_handler_data,
                        bool init_filters)
{
  memset (client, 0, sizeof (pkg_config_client_t));

  pkg_config_client_set_error_handler (
      client, error_handler, error_handler_data);

  pkg_config_client_set_sysroot_dir (client, NULL);
  pkg_config_client_set_buildroot_dir (client, NULL);
  pkg_config_client_set_prefix_varname (client, NULL);

  if (init_filters)
  {
    pkg_config_path_build_from_environ ("PKG_CONFIG_SYSTEM_LIBRARY_PATH",
                                        SYSTEM_LIBDIR,
                                        &client->filter_libdirs,
                                        false);
    pkg_config_path_build_from_environ ("PKG_CONFIG_SYSTEM_INCLUDE_PATH",
                                        SYSTEM_INCLUDEDIR,
                                        &client->filter_includedirs,
                                        false);

    /* GCC uses these environment variables to define system include paths, so
     * we should check them. */
    pkg_config_path_build_from_environ (
        "LIBRARY_PATH", NULL, &client->filter_libdirs, false);
    pkg_config_path_build_from_environ (
        "CPATH", NULL, &client->filter_includedirs, false);
    pkg_config_path_build_from_environ (
        "C_INCLUDE_PATH", NULL, &client->filter_includedirs, false);
    pkg_config_path_build_from_environ (
        "CPLUS_INCLUDE_PATH", NULL, &client->filter_includedirs, false);
    pkg_config_path_build_from_environ (
        "OBJC_INCLUDE_PATH", NULL, &client->filter_includedirs, false);

#ifdef _WIN32
    /* also use the path lists that MSVC uses on windows */
    pkg_config_path_build_from_environ (
        "INCLUDE", NULL, &client->filter_includedirs, false);
#endif
  }

  PKG_CONFIG_TRACE (client, "initialized client @%p", client);

#ifndef LIBPKG_CONFIG_NTRACE
  if (init_filters)
  {
    trace_path_list (
        client, "filtered library paths", &client->filter_libdirs);
    trace_path_list (
        client, "filtered include paths", &client->filter_includedirs);
  }
#endif
}

/*
 * !doc
 *
 * .. c:function:: pkg_config_client_t*
 * pkg_config_client_new(pkg_config_error_handler_func_t error_handler, void
 * *error_handler_data, bool init_filters)
 *
 *    Allocate and initialise a pkg-config client object.
 *
 *    :param pkg_config_error_handler_func_t error_handler: An optional error
 * handler to use for logging errors. :param void* error_handler_data: user
 * data passed to optional error handler :param bool init_filters: whether two
 * initialize include/lib path filters from environment/defaults :return: A
 * pkg-config client object. :rtype: pkg_config_client_t*
 */
pkg_config_client_t*
pkg_config_client_new (pkg_config_error_handler_func_t error_handler,
                       void* error_handler_data,
                       bool init_filters)
{
  pkg_config_client_t* out = malloc (sizeof (pkg_config_client_t));
  if (out != NULL)
    pkg_config_client_init (
        out, error_handler, error_handler_data, init_filters);
  return out;
}

/*
 * !doc
 *
 * .. c:function:: void pkg_config_client_deinit(pkg_config_client_t *client)
 *
 *    Release resources belonging to a pkg-config client object.
 *
 *    :param pkg_config_client_t* client: The client to deinitialise.
 *    :return: nothing
 */
void
pkg_config_client_deinit (pkg_config_client_t* client)
{
  PKG_CONFIG_TRACE (client, "deinit @%p", client);

  if (client->prefix_varname != NULL)
    free (client->prefix_varname);

  if (client->sysroot_dir != NULL)
    free (client->sysroot_dir);

  if (client->buildroot_dir != NULL)
    free (client->buildroot_dir);

  pkg_config_path_free (&client->filter_libdirs);
  pkg_config_path_free (&client->filter_includedirs);

  pkg_config_tuple_free_global (client);
  pkg_config_path_free (&client->dir_list);
  pkg_config_cache_free (client);
}

/*
 * !doc
 *
 * .. c:function:: void pkg_config_client_free(pkg_config_client_t *client)
 *
 *    Release resources belonging to a pkg-config client object and then free
 * the client object itself.
 *
 *    :param pkg_config_client_t* client: The client to deinitialise and free.
 *    :return: nothing
 */
void
pkg_config_client_free (pkg_config_client_t* client)
{
  pkg_config_client_deinit (client);
  free (client);
}

/*
 * !doc
 *
 * .. c:function:: const char *pkg_config_client_get_sysroot_dir(const
 * pkg_config_client_t *client)
 *
 *    Retrieves the client's sysroot directory (if any).
 *
 *    :param pkg_config_client_t* client: The client object being accessed.
 *    :return: A string containing the sysroot directory or NULL.
 *    :rtype: const char *
 */
const char*
pkg_config_client_get_sysroot_dir (const pkg_config_client_t* client)
{
  return client->sysroot_dir;
}

/*
 * !doc
 *
 * .. c:function:: void pkg_config_client_set_sysroot_dir(pkg_config_client_t
 * *client, const char *sysroot_dir)
 *
 *    Sets or clears the sysroot directory on a client object.  Any previous
 * sysroot directory setting is automatically released if one was previously
 * set.
 *
 *    Additionally, the global tuple ``$(pc_sysrootdir)`` is set as
 * appropriate based on the new setting.
 *
 *    :param pkg_config_client_t* client: The client object being modified.
 *    :param char* sysroot_dir: The sysroot directory to set or NULL to unset.
 *    :return: nothing
 */
void
pkg_config_client_set_sysroot_dir (pkg_config_client_t* client,
                                   const char* sysroot_dir)
{
  if (client->sysroot_dir != NULL)
    free (client->sysroot_dir);

  client->sysroot_dir = sysroot_dir != NULL ? strdup (sysroot_dir) : NULL;

  PKG_CONFIG_TRACE (client,
                    "set sysroot_dir to: %s",
                    client->sysroot_dir != NULL ? client->sysroot_dir
                                                : "<default>");

  pkg_config_tuple_add_global (
      client,
      "pc_sysrootdir",
      client->sysroot_dir != NULL ? client->sysroot_dir : "/");
}

/*
 * !doc
 *
 * .. c:function:: const char *pkg_config_client_get_buildroot_dir(const
 * pkg_config_client_t *client)
 *
 *    Retrieves the client's buildroot directory (if any).
 *
 *    :param pkg_config_client_t* client: The client object being accessed.
 *    :return: A string containing the buildroot directory or NULL.
 *    :rtype: const char *
 */
const char*
pkg_config_client_get_buildroot_dir (const pkg_config_client_t* client)
{
  return client->buildroot_dir;
}

/*
 * !doc
 *
 * .. c:function:: void
 * pkg_config_client_set_buildroot_dir(pkg_config_client_t *client, const char
 * *buildroot_dir)
 *
 *    Sets or clears the buildroot directory on a client object.  Any previous
 * buildroot directory setting is automatically released if one was previously
 * set.
 *
 *    Additionally, the global tuple ``$(pc_top_builddir)`` is set as
 * appropriate based on the new setting.
 *
 *    :param pkg_config_client_t* client: The client object being modified.
 *    :param char* buildroot_dir: The buildroot directory to set or NULL to
 * unset. :return: nothing
 */
void
pkg_config_client_set_buildroot_dir (pkg_config_client_t* client,
                                     const char* buildroot_dir)
{
  if (client->buildroot_dir != NULL)
    free (client->buildroot_dir);

  client->buildroot_dir =
      buildroot_dir != NULL ? strdup (buildroot_dir) : NULL;

  PKG_CONFIG_TRACE (client,
                    "set buildroot_dir to: %s",
                    client->buildroot_dir != NULL ? client->buildroot_dir
                                                  : "<default>");

  pkg_config_tuple_add_global (client,
                               "pc_top_builddir",
                               client->buildroot_dir != NULL
                                   ? client->buildroot_dir
                                   : "$(top_builddir)");
}

/*
 * !doc
 *
 * .. c:function:: void pkg_config_error(const pkg_config_client_t *client,
 *                                    unsigned int eflag,
 *                                    const char *format, ...)
 *
 *    Report an error to a client-registered error handler.
 *
 *    :param pkg_config_client_t* client: The pkg-config client object to
 * report the error to. :param unsigned int eflag: On of the
 * LIBPKG_CONFIG_ERRF_* error "flags" describing this error. :param char*
 * format: A printf-style format string to use for formatting the error
 * message. :return: nothing
 */
void
pkg_config_error (const pkg_config_client_t* client,
                  unsigned int eflag,
                  const char* filename,
                  size_t lineno,
                  const char* format,
                  ...)
{
  if (client->error_handler != NULL)
  {
    char buf[PKG_CONFIG_BUFSIZE];
    va_list va;

    va_start (va, format);
    vsnprintf (buf, sizeof buf, format, va);
    va_end (va);

    client->error_handler (eflag,
                           filename, lineno,
                           buf,
                           client, client->error_handler_data);
  }
}

/*
 * !doc
 *
 * .. c:function:: void pkg_config_warn(const pkg_config_client_t *client,
 * const char *format, ...)
 *
 *    Report an error to a client-registered warn handler.
 *
 *    :param pkg_config_client_t* client: The pkg-config client object to
 * report the error to. :param char* format: A printf-style format string to
 * use for formatting the warning message. :return: nothing
 */
void
pkg_config_warn (const pkg_config_client_t* client,
                 const char* filename,
                 size_t lineno,
                 const char* format, ...)
{
  if (client->warn_handler != NULL)
  {
    char buf[PKG_CONFIG_BUFSIZE];
    va_list va;

    va_start (va, format);
    vsnprintf (buf, sizeof buf, format, va);
    va_end (va);

    client->warn_handler (LIBPKG_CONFIG_ERRF_OK,
                          filename, lineno,
                          buf,
                          client, client->warn_handler_data);
  }
}

/*
 * !doc
 *
 * .. c:function:: bool pkg_config_trace(const pkg_config_client_t *client,
 * const char *filename, size_t len, const char *funcname, const char *format,
 * ...)
 *
 *    Report a message to a client-registered trace handler.
 *
 *    :param pkg_config_client_t* client: The pkg-config client object to
 * report the trace message to. :param char* filename: The file the function
 * is in. :param size_t lineno: The line number currently being executed.
 *    :param char* funcname: The function name to use.
 *    :param char* format: A printf-style format string to use for formatting
 * the trace message. :return: nothing
 */
void
pkg_config_trace (const pkg_config_client_t* client,
                  const char* filename,
                  size_t lineno,
                  const char* funcname,
                  const char* format,
                  ...)
{
  if (client == NULL || client->trace_handler == NULL)
    return;

#ifndef LIBPKG_CONFIG_NTRACE

  char buf[PKG_CONFIG_BUFSIZE];
  size_t len;
  va_list va;

  len = snprintf (buf, sizeof buf, "[%s]: ", funcname);

  va_start (va, format);
  vsnprintf (buf + len, sizeof (buf) - len, format, va);
  va_end (va);

  client->trace_handler (
      LIBPKG_CONFIG_ERRF_OK,
      filename, lineno,
      buf,
      client, client->trace_handler_data);
#else
  (void)client;
  (void)filename;
  (void)lineno;
  (void)funcname;
  (void)format;
#endif
}

/*
 * !doc
 *
 * .. c:function:: unsigned int pkg_config_client_get_flags(const
 * pkg_config_client_t *client)
 *
 *    Retrieves resolver-specific flags associated with a client object.
 *
 *    :param pkg_config_client_t* client: The client object to retrieve the
 * resolver-specific flags from. :return: a bitfield of resolver-specific
 * flags :rtype: uint
 */
unsigned int
pkg_config_client_get_flags (const pkg_config_client_t* client)
{
  return client->flags;
}

/*
 * !doc
 *
 * .. c:function:: void pkg_config_client_set_flags(pkg_config_client_t
 * *client, unsigned int flags)
 *
 *    Sets resolver-specific flags associated with a client object.
 *
 *    :param pkg_config_client_t* client: The client object to set the
 * resolver-specific flags on. :return: nothing
 */
void
pkg_config_client_set_flags (pkg_config_client_t* client, unsigned int flags)
{
  client->flags = flags;
}

/*
 * !doc
 *
 * .. c:function:: const char *pkg_config_client_get_prefix_varname(const
 * pkg_config_client_t *client)
 *
 *    Retrieves the name of the variable that should contain a module's
 * prefix. In some cases, it is necessary to override this variable to allow
 * proper path relocation.
 *
 *    :param pkg_config_client_t* client: The client object to retrieve the
 * prefix variable name from. :return: the prefix variable name as a string
 *    :rtype: const char *
 */
const char*
pkg_config_client_get_prefix_varname (const pkg_config_client_t* client)
{
  return client->prefix_varname;
}

/*
 * !doc
 *
 * .. c:function:: void
 * pkg_config_client_set_prefix_varname(pkg_config_client_t *client, const
 * char *prefix_varname)
 *
 *    Sets the name of the variable that should contain a module's prefix.
 *    If the variable name is ``NULL``, then the default variable name
 * (``prefix``) is used.
 *
 *    :param pkg_config_client_t* client: The client object to set the prefix
 * variable name on. :param char* prefix_varname: The prefix variable name to
 * set. :return: nothing
 */
void
pkg_config_client_set_prefix_varname (pkg_config_client_t* client,
                                      const char* prefix_varname)
{
  if (prefix_varname == NULL)
    prefix_varname = "prefix";

  if (client->prefix_varname != NULL)
    free (client->prefix_varname);

  client->prefix_varname = strdup (prefix_varname);

  PKG_CONFIG_TRACE (
      client, "set prefix_varname to: %s", client->prefix_varname);
}

/*
 * !doc
 *
 * .. c:function:: pkg_config_client_get_warn_handler(const
 * pkg_config_client_t *client)
 *
 *    Returns the warning handler if one is set, else ``NULL``.
 *
 *    :param pkg_config_client_t* client: The client object to get the warn
 * handler from. :return: a function pointer to the warn handler or ``NULL``
 */
pkg_config_error_handler_func_t
pkg_config_client_get_warn_handler (const pkg_config_client_t* client)
{
  return client->warn_handler;
}

/*
 * !doc
 *
 * .. c:function:: pkg_config_client_set_warn_handler(pkg_config_client_t
 * *client, pkg_config_error_handler_func_t warn_handler, void
 * *warn_handler_data)
 *
 *    Sets a warn handler on a client object or uninstalls one if set to
 * ``NULL``.
 *
 *    :param pkg_config_client_t* client: The client object to set the warn
 * handler on. :param pkg_config_error_handler_func_t warn_handler: The warn
 * handler to set. :param void* warn_handler_data: Optional data to associate
 * with the warn handler. :return: nothing
 */
void
pkg_config_client_set_warn_handler (
    pkg_config_client_t* client,
    pkg_config_error_handler_func_t warn_handler,
    void* warn_handler_data)
{
  client->warn_handler = warn_handler;
  client->warn_handler_data = warn_handler_data;
}

/*
 * !doc
 *
 * .. c:function:: pkg_config_client_get_error_handler(const
 * pkg_config_client_t *client)
 *
 *    Returns the error handler if one is set, else ``NULL``.
 *
 *    :param pkg_config_client_t* client: The client object to get the error
 * handler from. :return: a function pointer to the error handler or ``NULL``
 */
pkg_config_error_handler_func_t
pkg_config_client_get_error_handler (const pkg_config_client_t* client)
{
  return client->error_handler;
}

/*
 * !doc
 *
 * .. c:function:: pkg_config_client_set_error_handler(pkg_config_client_t
 * *client, pkg_config_error_handler_func_t error_handler, void
 * *error_handler_data)
 *
 *    Sets a warn handler on a client object or uninstalls one if set to
 * ``NULL``.
 *
 *    :param pkg_config_client_t* client: The client object to set the error
 * handler on. :param pkg_config_error_handler_func_t error_handler: The error
 * handler to set. :param void* error_handler_data: Optional data to associate
 * with the error handler. :return: nothing
 */
void
pkg_config_client_set_error_handler (
    pkg_config_client_t* client,
    pkg_config_error_handler_func_t error_handler,
    void* error_handler_data)
{
  client->error_handler = error_handler;
  client->error_handler_data = error_handler_data;
}

/*
 * !doc
 *
 * .. c:function:: pkg_config_client_get_trace_handler(const
 * pkg_config_client_t *client)
 *
 *    Returns the error handler if one is set, else ``NULL``.
 *
 *    :param pkg_config_client_t* client: The client object to get the error
 * handler from. :return: a function pointer to the error handler or ``NULL``
 */
pkg_config_error_handler_func_t
pkg_config_client_get_trace_handler (const pkg_config_client_t* client)
{
  return client->trace_handler;
}

/*
 * !doc
 *
 * .. c:function:: pkg_config_client_set_trace_handler(pkg_config_client_t
 * *client, pkg_config_error_handler_func_t trace_handler, void
 * *trace_handler_data)
 *
 *    Sets a warn handler on a client object or uninstalls one if set to
 * ``NULL``.
 *
 *    :param pkg_config_client_t* client: The client object to set the error
 * handler on. :param pkg_config_error_handler_func_t trace_handler: The error
 * handler to set. :param void* trace_handler_data: Optional data to associate
 * with the error handler. :return: nothing
 */
void
pkg_config_client_set_trace_handler (
    pkg_config_client_t* client,
    pkg_config_error_handler_func_t trace_handler,
    void* trace_handler_data)
{
  client->trace_handler = trace_handler;
  client->trace_handler_data = trace_handler_data;
}
