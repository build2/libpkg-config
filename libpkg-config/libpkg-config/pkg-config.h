/*
 * pkg-config.h
 * Global include file for everything public in libpkg-config.
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

#ifndef LIBPKG_CONFIG_PKG_CONFIG_H
#define LIBPKG_CONFIG_PKG_CONFIG_H

#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>

#include <libpkg-config/list.h> /* pkgconf_list_t */

#include <libpkg-config/version.h>
#include <libpkg-config/export.h>

#ifdef __cplusplus
//extern "C"
//{
#endif

/* pkg-config uses ';' on win32 as ':' is part of path */
#ifdef _WIN32
#define LIBPKG_CONFIG_PATH_SEP_S ";"
#else
#define LIBPKG_CONFIG_PATH_SEP_S ":"
#endif

#ifdef _WIN32
#define LIBPKG_CONFIG_DIR_SEP_S '\\'
#else
#define LIBPKG_CONFIG_DIR_SEP_S '/'
#endif

#ifdef _WIN32
#define realpath(N, R) _fullpath ((R), (N), _MAX_PATH)
#endif

typedef enum
{
  PKGCONF_CMP_NOT_EQUAL,
  PKGCONF_CMP_ANY,
  PKGCONF_CMP_LESS_THAN,
  PKGCONF_CMP_LESS_THAN_EQUAL,
  PKGCONF_CMP_EQUAL,
  PKGCONF_CMP_GREATER_THAN,
  PKGCONF_CMP_GREATER_THAN_EQUAL
} pkgconf_pkg_comparator_t;

typedef struct pkgconf_pkg_ pkgconf_pkg_t;
typedef struct pkgconf_dependency_ pkgconf_dependency_t;
typedef struct pkgconf_tuple_ pkgconf_tuple_t;
typedef struct pkgconf_fragment_ pkgconf_fragment_t;
typedef struct pkgconf_path_ pkgconf_path_t;
typedef struct pkgconf_client_ pkgconf_client_t;

struct pkgconf_fragment_
{
  pkgconf_node_t iter;

  char type;
  char* data;

  bool merged;
};

struct pkgconf_dependency_
{
  pkgconf_node_t iter;

  char* package;
  pkgconf_pkg_comparator_t compare;
  char* version;
  pkgconf_pkg_t* parent;
  pkgconf_pkg_t* match;

  unsigned int flags; /* LIBPKG_CONFIG_PKG_DEPF_* */
};

#define LIBPKG_CONFIG_PKG_DEPF_INTERNAL 0x01

struct pkgconf_tuple_
{
  pkgconf_node_t iter;

  char* key;
  char* value;
};

struct pkgconf_path_
{
  pkgconf_node_t lnode;

  char* path;
  void* handle_path;
  void* handle_device;
};

struct pkgconf_pkg_
{
  pkgconf_node_t cache_iter;

  int refcount; /* Negative means static object. */
  char* id;
  char* filename;
  char* realname;
  char* version;
  char* description;
  char* url;
  char* pc_filedir;

  pkgconf_list_t libs;
  pkgconf_list_t libs_private;
  pkgconf_list_t cflags;
  pkgconf_list_t cflags_private;

  pkgconf_list_t required; /* this used to be requires but that is now a
                              reserved keyword */
  pkgconf_list_t requires_private;
  pkgconf_list_t conflicts;

  pkgconf_list_t vars;

  unsigned int flags; /* LIBPKG_CONFIG_PKG_PROPF_* */

  pkgconf_client_t* owner;

  /* these resources are owned by the package and do not need special
   * management, under no circumstance attempt to allocate or free objects
   * belonging to these pointers
   */
  pkgconf_tuple_t* orig_prefix;
  pkgconf_tuple_t* prefix;
};

#define LIBPKG_CONFIG_PKG_PROPF_NONE        0x00
#define LIBPKG_CONFIG_PKG_PROPF_CONST       0x01
#define LIBPKG_CONFIG_PKG_PROPF_CACHED      0x02
#define LIBPKG_CONFIG_PKG_PROPF_SEEN        0x04
#define LIBPKG_CONFIG_PKG_PROPF_UNINSTALLED 0x08

typedef bool (*pkgconf_pkg_iteration_func_t) (const pkgconf_pkg_t* pkg,
                                              void* data);
typedef void (*pkgconf_pkg_traverse_func_t) (pkgconf_client_t* client,
                                             pkgconf_pkg_t* pkg,
                                             void* data);

/* The eflag argument is one of LIBPKG_CONFIG_PKG_ERRF_* "flags" for
   errors and 0 (*_OK) for warnings/traces. */
typedef void (*pkgconf_error_handler_func_t) (unsigned int eflag,
                                              const char* msg,
                                              const pkgconf_client_t* client,
                                              const void* data);

struct pkgconf_client_
{
  pkgconf_list_t dir_list;
  pkgconf_list_t pkg_cache;

  pkgconf_list_t filter_libdirs;
  pkgconf_list_t filter_includedirs;

  pkgconf_list_t global_vars;

  void* error_handler_data;
  void* warn_handler_data;
  void* trace_handler_data;

  pkgconf_error_handler_func_t error_handler;
  pkgconf_error_handler_func_t warn_handler;
  pkgconf_error_handler_func_t trace_handler;

  char* sysroot_dir;
  char* buildroot_dir;

  unsigned int flags; /* LIBPKG_CONFIG_PKG_PKGF_* */

  char* prefix_varname;
};

#define LIBPKG_CONFIG_PKG_PKGF_NONE                        0x0000
#define LIBPKG_CONFIG_PKG_PKGF_SEARCH_PRIVATE              0x0001
#define LIBPKG_CONFIG_PKG_PKGF_ENV_ONLY                    0x0002
#define LIBPKG_CONFIG_PKG_PKGF_NO_UNINSTALLED              0x0004
#define LIBPKG_CONFIG_PKG_PKGF_MERGE_PRIVATE_FRAGMENTS     0x0008
#define LIBPKG_CONFIG_PKG_PKGF_SKIP_CONFLICTS              0x0010
#define LIBPKG_CONFIG_PKG_PKGF_NO_CACHE                    0x0020
#define LIBPKG_CONFIG_PKG_PKGF_SKIP_ERRORS                 0x0040
#define LIBPKG_CONFIG_PKG_PKGF_ITER_PKG_IS_PRIVATE         0x0080
#define LIBPKG_CONFIG_PKG_PKGF_REDEFINE_PREFIX             0x0100
#define LIBPKG_CONFIG_PKG_PKGF_DONT_RELOCATE_PATHS         0x0200
#define LIBPKG_CONFIG_PKG_PKGF_DONT_FILTER_INTERNAL_CFLAGS 0x0400
#define LIBPKG_CONFIG_PKG_PKGF_MERGE_SPECIAL_FRAGMENTS     0x0800
#define LIBPKG_CONFIG_PKG_PKGF_FDO_SYSROOT_RULES           0x1000

/* client.c */
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_client_init (pkgconf_client_t* client,
                     pkgconf_error_handler_func_t error_handler,
                     void* error_handler_data,
                     bool init_filters);
LIBPKG_CONFIG_SYMEXPORT pkgconf_client_t*
pkgconf_client_new (pkgconf_error_handler_func_t error_handler,
                    void* error_handler_data,
                    bool init_filters);
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_client_deinit (pkgconf_client_t* client);
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_client_free (pkgconf_client_t* client);
LIBPKG_CONFIG_SYMEXPORT const char*
pkgconf_client_get_sysroot_dir (const pkgconf_client_t* client);
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_client_set_sysroot_dir (pkgconf_client_t* client,
                                const char* sysroot_dir);
LIBPKG_CONFIG_SYMEXPORT const char*
pkgconf_client_get_buildroot_dir (const pkgconf_client_t* client);
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_client_set_buildroot_dir (pkgconf_client_t* client,
                                  const char* buildroot_dir);
LIBPKG_CONFIG_SYMEXPORT unsigned int
pkgconf_client_get_flags (const pkgconf_client_t* client);
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_client_set_flags (pkgconf_client_t* client, unsigned int flags);
LIBPKG_CONFIG_SYMEXPORT const char*
pkgconf_client_get_prefix_varname (const pkgconf_client_t* client);
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_client_set_prefix_varname (pkgconf_client_t* client,
                                   const char* prefix_varname);
LIBPKG_CONFIG_SYMEXPORT pkgconf_error_handler_func_t
pkgconf_client_get_warn_handler (const pkgconf_client_t* client);
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_client_set_warn_handler (pkgconf_client_t* client,
                                 pkgconf_error_handler_func_t warn_handler,
                                 void* warn_handler_data);
LIBPKG_CONFIG_SYMEXPORT pkgconf_error_handler_func_t
pkgconf_client_get_error_handler (const pkgconf_client_t* client);
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_client_set_error_handler (pkgconf_client_t* client,
                                  pkgconf_error_handler_func_t error_handler,
                                  void* error_handler_data);
LIBPKG_CONFIG_SYMEXPORT pkgconf_error_handler_func_t
pkgconf_client_get_trace_handler (const pkgconf_client_t* client);
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_client_set_trace_handler (pkgconf_client_t* client,
                                  pkgconf_error_handler_func_t trace_handler,
                                  void* trace_handler_data);
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_client_dir_list_build (pkgconf_client_t* client);

/* Note that MinGW's printf() format semantics have changed starting GCC 10
 * (see stdinc.h for details).
 */
#if defined(__GNUC__) || defined(__INTEL_COMPILER)
#  if defined(_WIN32) && defined(__GNUC__) && __GNUC__ >= 10
#    define LIBPKG_CONFIG_PRINTFLIKE(fmtarg, firstvararg)                   \
  __attribute__ ((__format__ (gnu_printf, fmtarg, firstvararg)))
#  else
#    define LIBPKG_CONFIG_PRINTFLIKE(fmtarg, firstvararg)                   \
  __attribute__ ((__format__ (__printf__, fmtarg, firstvararg)))
#  endif
#else
#  define LIBPKG_CONFIG_PRINTFLIKE(fmtarg, firstvararg)
#endif /* defined(__INTEL_COMPILER) || defined(__GNUC__) */

/* Note that if the library is build with LIBPKG_CONFIG_NTRACE, tracing will
   be disabled at compile time. */
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_error (const pkgconf_client_t* client,
               unsigned int eflag,
               const char* format, ...) LIBPKG_CONFIG_PRINTFLIKE (3, 4);
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_warn (const pkgconf_client_t* client,
              const char* format, ...) LIBPKG_CONFIG_PRINTFLIKE (2, 3);
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_trace (const pkgconf_client_t* client,
               const char* filename,
               size_t lineno,
               const char* funcname,
               const char* format,
               ...) LIBPKG_CONFIG_PRINTFLIKE (5, 6);

/* parser.c */
typedef void (*pkgconf_parser_operand_func_t) (void* data,
                                               const size_t lineno,
                                               const char* key,
                                               const char* value);
typedef void (*pkgconf_parser_warn_func_t) (void* data,
                                            const char* fmt,
                                            ...);

LIBPKG_CONFIG_SYMEXPORT void
pkgconf_parser_parse (FILE* f,
                      void* data,
                      const pkgconf_parser_operand_func_t* ops,
                      const pkgconf_parser_warn_func_t warnfunc,
                      const char* filename);

/* pkg.c */

/* Errors/flags that are either returned or set; see eflags arguments. */
#define LIBPKG_CONFIG_PKG_ERRF_OK                   0x00
#define LIBPKG_CONFIG_PKG_ERRF_PACKAGE_NOT_FOUND    0x01
#define LIBPKG_CONFIG_PKG_ERRF_PACKAGE_VER_MISMATCH 0x02
#define LIBPKG_CONFIG_PKG_ERRF_PACKAGE_CONFLICT     0x04

LIBPKG_CONFIG_SYMEXPORT pkgconf_pkg_t*
pkgconf_pkg_ref (pkgconf_client_t* client, pkgconf_pkg_t* pkg);
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_pkg_unref (pkgconf_client_t* client, pkgconf_pkg_t* pkg);
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_pkg_free (pkgconf_client_t* client, pkgconf_pkg_t* pkg);
LIBPKG_CONFIG_SYMEXPORT pkgconf_pkg_t*
pkgconf_pkg_find (pkgconf_client_t* client, const char* name);
LIBPKG_CONFIG_SYMEXPORT unsigned int
pkgconf_pkg_traverse (pkgconf_client_t* client,
                      pkgconf_pkg_t* root,
                      pkgconf_pkg_traverse_func_t func,
                      void* data,
                      int maxdepth,
                      unsigned int skip_flags);
LIBPKG_CONFIG_SYMEXPORT unsigned int
pkgconf_pkg_verify_graph (pkgconf_client_t* client,
                          pkgconf_pkg_t* root,
                          int depth);
LIBPKG_CONFIG_SYMEXPORT pkgconf_pkg_t*
pkgconf_pkg_verify_dependency (pkgconf_client_t* client,
                               pkgconf_dependency_t* pkgdep,
                               unsigned int* eflags);
LIBPKG_CONFIG_SYMEXPORT const char*
pkgconf_pkg_get_comparator (const pkgconf_dependency_t* pkgdep);
LIBPKG_CONFIG_SYMEXPORT unsigned int
pkgconf_pkg_cflags (pkgconf_client_t* client,
                    pkgconf_pkg_t* root,
                    pkgconf_list_t* list,
                    int maxdepth);
LIBPKG_CONFIG_SYMEXPORT unsigned int
pkgconf_pkg_libs (pkgconf_client_t* client,
                  pkgconf_pkg_t* root,
                  pkgconf_list_t* list,
                  int maxdepth);
LIBPKG_CONFIG_SYMEXPORT pkgconf_pkg_comparator_t
pkgconf_pkg_comparator_lookup_by_name (const char* name);
LIBPKG_CONFIG_SYMEXPORT const pkgconf_pkg_t*
pkgconf_builtin_pkg_get (const char* name);

LIBPKG_CONFIG_SYMEXPORT int
pkgconf_compare_version (const char* a, const char* b);
LIBPKG_CONFIG_SYMEXPORT pkgconf_pkg_t*
pkgconf_scan_all (pkgconf_client_t* client,
                  void* ptr,
                  pkgconf_pkg_iteration_func_t func);

/* parse.c */
LIBPKG_CONFIG_SYMEXPORT pkgconf_pkg_t*
pkgconf_pkg_new_from_file (pkgconf_client_t* client,
                           const char* path,
                           FILE* f);
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_dependency_parse_str (const pkgconf_client_t* client,
                              pkgconf_list_t* deplist_head,
                              const char* depends,
                              unsigned int flags);
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_dependency_parse (const pkgconf_client_t* client,
                          pkgconf_pkg_t* pkg,
                          pkgconf_list_t* deplist_head,
                          const char* depends,
                          unsigned int flags);
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_dependency_append (pkgconf_list_t* list,
                           pkgconf_dependency_t* tail);
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_dependency_free (pkgconf_list_t* list);
LIBPKG_CONFIG_SYMEXPORT pkgconf_dependency_t*
pkgconf_dependency_add (const pkgconf_client_t* client,
                        pkgconf_list_t* list,
                        const char* package,
                        const char* version,
                        pkgconf_pkg_comparator_t compare,
                        unsigned int flags);

/* argvsplit.c */
LIBPKG_CONFIG_SYMEXPORT int
pkgconf_argv_split (const char* src, int* argc, char*** argv);
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_argv_free (char** argv);

/* fragment.c */
typedef struct pkgconf_fragment_render_ops_
{
  size_t (*render_len) (const pkgconf_list_t* list, bool escape);
  void (*render_buf) (const pkgconf_list_t* list,
                      char* buf,
                      size_t len,
                      bool escape);
} pkgconf_fragment_render_ops_t;

typedef bool (*pkgconf_fragment_filter_func_t) (
  const pkgconf_client_t* client,
  const pkgconf_fragment_t* frag,
  void* data);
LIBPKG_CONFIG_SYMEXPORT bool
pkgconf_fragment_parse (const pkgconf_client_t* client,
                        pkgconf_list_t* list,
                        pkgconf_list_t* vars,
                        const char* value);
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_fragment_add (const pkgconf_client_t* client,
                      pkgconf_list_t* list,
                      const char* string);
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_fragment_copy (const pkgconf_client_t* client,
                       pkgconf_list_t* list,
                       const pkgconf_fragment_t* base,
                       bool is_private);
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_fragment_copy_list (const pkgconf_client_t* client,
                            pkgconf_list_t* list,
                            const pkgconf_list_t* base);
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_fragment_delete (pkgconf_list_t* list, pkgconf_fragment_t* node);
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_fragment_free (pkgconf_list_t* list);
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_fragment_filter (const pkgconf_client_t* client,
                         pkgconf_list_t* dest,
                         pkgconf_list_t* src,
                         pkgconf_fragment_filter_func_t filter_func,
                         void* data);
LIBPKG_CONFIG_SYMEXPORT size_t
pkgconf_fragment_render_len (const pkgconf_list_t* list,
                             bool escape,
                             const pkgconf_fragment_render_ops_t* ops);
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_fragment_render_buf (const pkgconf_list_t* list,
                             char* buf,
                             size_t len,
                             bool escape,
                             const pkgconf_fragment_render_ops_t* ops);
LIBPKG_CONFIG_SYMEXPORT char*
pkgconf_fragment_render (const pkgconf_list_t* list,
                         bool escape,
                         const pkgconf_fragment_render_ops_t* ops);
LIBPKG_CONFIG_SYMEXPORT bool
pkgconf_fragment_has_system_dir (const pkgconf_client_t* client,
                                 const pkgconf_fragment_t* frag);

/* fileio.c */
LIBPKG_CONFIG_SYMEXPORT char*
pkgconf_fgetline (char* line, size_t size, FILE* stream);

/* tuple.c */
LIBPKG_CONFIG_SYMEXPORT pkgconf_tuple_t*
pkgconf_tuple_add (const pkgconf_client_t* client,
                   pkgconf_list_t* parent,
                   const char* key,
                   const char* value,
                   bool parse);
LIBPKG_CONFIG_SYMEXPORT char*
pkgconf_tuple_find (const pkgconf_client_t* client,
                    pkgconf_list_t* list,
                    const char* key);
LIBPKG_CONFIG_SYMEXPORT char*
pkgconf_tuple_parse (const pkgconf_client_t* client,
                     pkgconf_list_t* list,
                     const char* value);
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_tuple_free (pkgconf_list_t* list);
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_tuple_free_entry (pkgconf_tuple_t* tuple, pkgconf_list_t* list);
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_tuple_add_global (pkgconf_client_t* client,
                          const char* key,
                          const char* value);
LIBPKG_CONFIG_SYMEXPORT char*
pkgconf_tuple_find_global (const pkgconf_client_t* client, const char* key);
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_tuple_free_global (pkgconf_client_t* client);
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_tuple_define_global (pkgconf_client_t* client, const char* kv);

/* cache.c */
LIBPKG_CONFIG_SYMEXPORT pkgconf_pkg_t*
pkgconf_cache_lookup (pkgconf_client_t* client, const char* id);
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_cache_add (pkgconf_client_t* client, pkgconf_pkg_t* pkg);
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_cache_remove (pkgconf_client_t* client, pkgconf_pkg_t* pkg);
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_cache_free (pkgconf_client_t* client);

/* path.c */
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_path_add (const char* text, pkgconf_list_t* dirlist, bool filter);
LIBPKG_CONFIG_SYMEXPORT size_t
pkgconf_path_split (const char* text, pkgconf_list_t* dirlist, bool filter);
LIBPKG_CONFIG_SYMEXPORT size_t
pkgconf_path_build_from_environ (const char* envvarname,
                                 const char* fallback,
                                 pkgconf_list_t* dirlist,
                                 bool filter);
LIBPKG_CONFIG_SYMEXPORT bool
pkgconf_path_match_list (const char* path, const pkgconf_list_t* dirlist);
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_path_free (pkgconf_list_t* dirlist);
LIBPKG_CONFIG_SYMEXPORT bool
pkgconf_path_relocate (char* buf, size_t buflen);
LIBPKG_CONFIG_SYMEXPORT void
pkgconf_path_copy_list (pkgconf_list_t* dst, const pkgconf_list_t* src);

#ifdef __cplusplus
}
#endif

#endif /* LIBPKG_CONFIG_PKG_CONFIG_H */
