/*
 * pkg-config.h
 * Global include file for everything public in libpkg-config.
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

#ifndef LIBPKG_CONFIG_PKG_CONFIG_H
#define LIBPKG_CONFIG_PKG_CONFIG_H

#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>

#include <libpkg-config/list.h> /* pkg_config_list_t */

#include <libpkg-config/version.h>
#include <libpkg-config/export.h>

#ifdef __cplusplus
extern "C"
{
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
  PKG_CONFIG_CMP_NOT_EQUAL,
  PKG_CONFIG_CMP_ANY,
  PKG_CONFIG_CMP_LESS_THAN,
  PKG_CONFIG_CMP_LESS_THAN_EQUAL,
  PKG_CONFIG_CMP_EQUAL,
  PKG_CONFIG_CMP_GREATER_THAN,
  PKG_CONFIG_CMP_GREATER_THAN_EQUAL
} pkg_config_pkg_comparator_t;

typedef struct pkg_config_pkg_ pkg_config_pkg_t;
typedef struct pkg_config_dependency_ pkg_config_dependency_t;
typedef struct pkg_config_tuple_ pkg_config_tuple_t;
typedef struct pkg_config_fragment_ pkg_config_fragment_t;
typedef struct pkg_config_path_ pkg_config_path_t;
typedef struct pkg_config_client_ pkg_config_client_t;

struct pkg_config_fragment_
{
  pkg_config_node_t iter;

  char type;
  char* data;

  bool merged;
};

struct pkg_config_dependency_
{
  pkg_config_node_t iter;

  char* package;
  pkg_config_pkg_comparator_t compare;
  char* version;
  pkg_config_pkg_t* parent;
  pkg_config_pkg_t* match;

  unsigned int flags; /* LIBPKG_CONFIG_PKG_DEPF_* */
};

#define LIBPKG_CONFIG_PKG_DEPF_INTERNAL 0x01

struct pkg_config_tuple_
{
  pkg_config_node_t iter;

  char* key;
  char* value;
};

struct pkg_config_path_
{
  pkg_config_node_t lnode;

  char* path;
  void* handle_path;
  void* handle_device;
};

struct pkg_config_pkg_
{
  pkg_config_node_t cache_iter;

  int refcount; /* Negative means static object. */
  char* id;
  char* filename;
  char* realname;
  char* version;
  char* description;
  char* url;
  char* pc_filedir;

  pkg_config_list_t libs;
  pkg_config_list_t libs_private;
  pkg_config_list_t cflags;
  pkg_config_list_t cflags_private;

  pkg_config_list_t required; /* this used to be requires but that is now a
                              reserved keyword */
  pkg_config_list_t requires_private;
  pkg_config_list_t conflicts;

  pkg_config_list_t vars;

  unsigned int flags; /* LIBPKG_CONFIG_PKG_PROPF_* */

  pkg_config_client_t* owner;

  /* these resources are owned by the package and do not need special
   * management, under no circumstance attempt to allocate or free objects
   * belonging to these pointers
   */
  pkg_config_tuple_t* orig_prefix;
  pkg_config_tuple_t* prefix;
};

#define LIBPKG_CONFIG_PKG_PROPF_NONE        0x00
#define LIBPKG_CONFIG_PKG_PROPF_CONST       0x01
#define LIBPKG_CONFIG_PKG_PROPF_CACHED      0x02
#define LIBPKG_CONFIG_PKG_PROPF_SEEN        0x04
#define LIBPKG_CONFIG_PKG_PROPF_UNINSTALLED 0x08

typedef bool (*pkg_config_pkg_iteration_func_t) (const pkg_config_pkg_t* pkg,
                                                 void* data);
typedef void (*pkg_config_pkg_traverse_func_t) (pkg_config_client_t* client,
                                                pkg_config_pkg_t* pkg,
                                                void* data);

/* The eflag argument is one of LIBPKG_CONFIG_ERRF_* "flags" for
   errors and 0 (*_OK) for warnings/traces.

   The filename may be NULL if there is no file position (in which case lineno
   is meaningless).
*/
typedef void (*pkg_config_error_handler_func_t) (
  unsigned int eflag,
  const char* filename,
  size_t lineno,
  const char* msg,
  const pkg_config_client_t* client,
  const void* data);

struct pkg_config_client_
{
  pkg_config_list_t dir_list;
  pkg_config_list_t pkg_cache;

  pkg_config_list_t filter_libdirs;
  pkg_config_list_t filter_includedirs;

  pkg_config_list_t global_vars;

  void* error_handler_data;
  void* warn_handler_data;
  void* trace_handler_data;

  pkg_config_error_handler_func_t error_handler;
  pkg_config_error_handler_func_t warn_handler;
  pkg_config_error_handler_func_t trace_handler;

  char* sysroot_dir;
  char* buildroot_dir;

  unsigned int flags; /* LIBPKG_CONFIG_PKG_PKGF_* */

  char* prefix_varname;
};

#define LIBPKG_CONFIG_PKG_PKGF_NONE                        0x0000
#define LIBPKG_CONFIG_PKG_PKGF_SEARCH_PRIVATE              0x0001
#define LIBPKG_CONFIG_PKG_PKGF_ENV_ONLY                    0x0002
#define LIBPKG_CONFIG_PKG_PKGF_CONSIDER_UNINSTALLED        0x0004
#define LIBPKG_CONFIG_PKG_PKGF_ADD_PRIVATE_FRAGMENTS       0x0008
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
pkg_config_client_init (pkg_config_client_t* client,
                        pkg_config_error_handler_func_t error_handler,
                        void* error_handler_data,
                        bool init_filters);
LIBPKG_CONFIG_SYMEXPORT pkg_config_client_t*
pkg_config_client_new (pkg_config_error_handler_func_t error_handler,
                       void* error_handler_data,
                       bool init_filters);
LIBPKG_CONFIG_SYMEXPORT void
pkg_config_client_deinit (pkg_config_client_t* client);
LIBPKG_CONFIG_SYMEXPORT void
pkg_config_client_free (pkg_config_client_t* client);
LIBPKG_CONFIG_SYMEXPORT const char*
pkg_config_client_get_sysroot_dir (const pkg_config_client_t* client);
LIBPKG_CONFIG_SYMEXPORT void
pkg_config_client_set_sysroot_dir (pkg_config_client_t* client,
                                   const char* sysroot_dir);
LIBPKG_CONFIG_SYMEXPORT const char*
pkg_config_client_get_buildroot_dir (const pkg_config_client_t* client);
LIBPKG_CONFIG_SYMEXPORT void
pkg_config_client_set_buildroot_dir (pkg_config_client_t* client,
                                     const char* buildroot_dir);
LIBPKG_CONFIG_SYMEXPORT unsigned int
pkg_config_client_get_flags (const pkg_config_client_t* client);
LIBPKG_CONFIG_SYMEXPORT void
pkg_config_client_set_flags (pkg_config_client_t* client, unsigned int flags);
LIBPKG_CONFIG_SYMEXPORT const char*
pkg_config_client_get_prefix_varname (const pkg_config_client_t* client);
LIBPKG_CONFIG_SYMEXPORT void
pkg_config_client_set_prefix_varname (pkg_config_client_t* client,
                                      const char* prefix_varname);
LIBPKG_CONFIG_SYMEXPORT pkg_config_error_handler_func_t
pkg_config_client_get_warn_handler (const pkg_config_client_t* client);
LIBPKG_CONFIG_SYMEXPORT void
pkg_config_client_set_warn_handler (pkg_config_client_t* client,
                                    pkg_config_error_handler_func_t warn_handler,
                                    void* warn_handler_data);
LIBPKG_CONFIG_SYMEXPORT pkg_config_error_handler_func_t
pkg_config_client_get_error_handler (const pkg_config_client_t* client);
LIBPKG_CONFIG_SYMEXPORT void
pkg_config_client_set_error_handler (pkg_config_client_t* client,
                                     pkg_config_error_handler_func_t error_handler,
                                     void* error_handler_data);
LIBPKG_CONFIG_SYMEXPORT pkg_config_error_handler_func_t
pkg_config_client_get_trace_handler (const pkg_config_client_t* client);
LIBPKG_CONFIG_SYMEXPORT void
pkg_config_client_set_trace_handler (pkg_config_client_t* client,
                                     pkg_config_error_handler_func_t trace_handler,
                                     void* trace_handler_data);
LIBPKG_CONFIG_SYMEXPORT void
pkg_config_client_dir_list_build (pkg_config_client_t* client);

/* Errors/flags that are either returned or set; see eflags arguments. */
#define LIBPKG_CONFIG_ERRF_OK                   0x00
#define LIBPKG_CONFIG_ERRF_MEMORY               0x01
#define LIBPKG_CONFIG_ERRF_PACKAGE_NOT_FOUND    0x02
#define LIBPKG_CONFIG_ERRF_PACKAGE_INVALID      0x04
#define LIBPKG_CONFIG_ERRF_PACKAGE_VER_MISMATCH 0x08
#define LIBPKG_CONFIG_ERRF_PACKAGE_CONFLICT     0x10
#define LIBPKG_CONFIG_ERRF_FILE_INVALID_SYNTAX  0x20
#define LIBPKG_CONFIG_ERRF_FILE_MISSING_FIELD   0x40

/* Note that MinGW's printf() format semantics have changed starting GCC 10.
 * In particular, GCC 10 complains about MSVC's 'I64' length modifier but now
 * accepts the standard (C99) 'z' modifier.
 */
#ifdef _WIN32
#  if defined(__GNUC__) && __GNUC__ >= 10
#    define LIBPKG_CONFIG_SIZE_FMT "%zu"
#  else
#    ifdef _WIN64
#      define LIBPKG_CONFIG_SIZE_FMT "%I64u"
#    else
#      define LIBPKG_CONFIG_SIZE_FMT "%u"
#    endif
#  endif
#else
#  define LIBPKG_CONFIG_SIZE_FMT "%zu"
#endif

/* Note that MinGW's printf() format semantics have changed starting GCC 10
 * (see above for details).
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
pkg_config_error (const pkg_config_client_t* client,
                  unsigned int eflag,
                  const char* filename,
                  size_t lineno,
                  const char* format, ...) LIBPKG_CONFIG_PRINTFLIKE (5, 6);
LIBPKG_CONFIG_SYMEXPORT void
pkg_config_warn (const pkg_config_client_t* client,
                 const char* filename,
                 size_t lineno,
                 const char* format, ...) LIBPKG_CONFIG_PRINTFLIKE (4, 5);
LIBPKG_CONFIG_SYMEXPORT void
pkg_config_trace (const pkg_config_client_t* client,
                  const char* filename,
                  size_t lineno,
                  const char* funcname,
                  const char* format,
                  ...) LIBPKG_CONFIG_PRINTFLIKE (5, 6);

/* parser.c */
typedef unsigned int /* eflags */ (*pkg_config_parser_operand_func_t) (
  void* data,
  const size_t lineno,
  const char* key,
  const char* value);

LIBPKG_CONFIG_SYMEXPORT unsigned int /* eflags */
pkg_config_parser_parse (pkg_config_client_t* client,
                         FILE* f,
                         void* data,
                         const pkg_config_parser_operand_func_t* ops,
                         size_t ops_count,
                         const char* filename);

/* pkg.c */

LIBPKG_CONFIG_SYMEXPORT pkg_config_pkg_t*
pkg_config_pkg_ref (pkg_config_client_t* client, pkg_config_pkg_t* pkg);
LIBPKG_CONFIG_SYMEXPORT void
pkg_config_pkg_unref (pkg_config_client_t* client, pkg_config_pkg_t* pkg);
LIBPKG_CONFIG_SYMEXPORT void
pkg_config_pkg_free (pkg_config_client_t* client, pkg_config_pkg_t* pkg);
LIBPKG_CONFIG_SYMEXPORT pkg_config_pkg_t*
pkg_config_pkg_find (pkg_config_client_t* client,
                     const char* name,
                     unsigned int* eflags);
LIBPKG_CONFIG_SYMEXPORT unsigned int
pkg_config_pkg_traverse (pkg_config_client_t* client,
                         pkg_config_pkg_t* root,
                         pkg_config_pkg_traverse_func_t func,
                         void* data,
                         int maxdepth,
                         unsigned int skip_flags);
LIBPKG_CONFIG_SYMEXPORT unsigned int
pkg_config_pkg_verify_graph (pkg_config_client_t* client,
                             pkg_config_pkg_t* root,
                             int depth);

/* The eflags argument is one or more of LIBPKG_CONFIG_ERRF_PACKAGE_*. */
LIBPKG_CONFIG_SYMEXPORT pkg_config_pkg_t*
pkg_config_pkg_verify_dependency (pkg_config_client_t* client,
                                  pkg_config_dependency_t* pkgdep,
                                  unsigned int* eflags);

LIBPKG_CONFIG_SYMEXPORT const char*
pkg_config_pkg_get_comparator (const pkg_config_dependency_t* pkgdep);
LIBPKG_CONFIG_SYMEXPORT unsigned int
pkg_config_pkg_cflags (pkg_config_client_t* client,
                       pkg_config_pkg_t* root,
                       pkg_config_list_t* list,
                       int maxdepth);
LIBPKG_CONFIG_SYMEXPORT unsigned int
pkg_config_pkg_libs (pkg_config_client_t* client,
                     pkg_config_pkg_t* root,
                     pkg_config_list_t* list,
                     int maxdepth);
LIBPKG_CONFIG_SYMEXPORT pkg_config_pkg_comparator_t
pkg_config_pkg_comparator_lookup_by_name (const char* name);
LIBPKG_CONFIG_SYMEXPORT const pkg_config_pkg_t*
pkg_config_builtin_pkg_get (const char* name);

LIBPKG_CONFIG_SYMEXPORT int
pkg_config_compare_version (const char* a, const char* b);
LIBPKG_CONFIG_SYMEXPORT pkg_config_pkg_t*
pkg_config_scan_all (pkg_config_client_t* client,
                     void* ptr,
                     pkg_config_pkg_iteration_func_t func);

/* parse.c */
LIBPKG_CONFIG_SYMEXPORT pkg_config_pkg_t*
pkg_config_pkg_new_from_file (pkg_config_client_t* client,
                              const char* path,
                              FILE* f,
                              unsigned int* eflags);
LIBPKG_CONFIG_SYMEXPORT void
pkg_config_dependency_parse_str (const pkg_config_client_t* client,
                                 pkg_config_list_t* deplist_head,
                                 const char* depends,
                                 unsigned int flags);
LIBPKG_CONFIG_SYMEXPORT void
pkg_config_dependency_parse (const pkg_config_client_t* client,
                             pkg_config_pkg_t* pkg,
                             pkg_config_list_t* deplist_head,
                             const char* depends,
                             unsigned int flags);
LIBPKG_CONFIG_SYMEXPORT void
pkg_config_dependency_append (pkg_config_list_t* list,
                              pkg_config_dependency_t* tail);
LIBPKG_CONFIG_SYMEXPORT void
pkg_config_dependency_free (pkg_config_list_t* list);
LIBPKG_CONFIG_SYMEXPORT pkg_config_dependency_t*
pkg_config_dependency_add (const pkg_config_client_t* client,
                           pkg_config_list_t* list,
                           const char* package,
                           const char* version,
                           pkg_config_pkg_comparator_t compare,
                           unsigned int flags);

/* argvsplit.c */
LIBPKG_CONFIG_SYMEXPORT int
pkg_config_argv_split (const char* src, int* argc, char*** argv);
LIBPKG_CONFIG_SYMEXPORT void
pkg_config_argv_free (char** argv);

/* fragment.c */
typedef struct pkg_config_fragment_render_ops_
{
  size_t (*render_len) (const pkg_config_list_t* list, bool escape);
  void (*render_buf) (const pkg_config_list_t* list,
                      char* buf,
                      size_t len,
                      bool escape);
} pkg_config_fragment_render_ops_t;

typedef bool (*pkg_config_fragment_filter_func_t) (
  const pkg_config_client_t* client,
  const pkg_config_fragment_t* frag,
  void* data);
LIBPKG_CONFIG_SYMEXPORT bool
pkg_config_fragment_parse (const pkg_config_client_t* client,
                           pkg_config_list_t* list,
                           pkg_config_list_t* vars,
                           const char* value);
LIBPKG_CONFIG_SYMEXPORT void
pkg_config_fragment_add (const pkg_config_client_t* client,
                         pkg_config_list_t* list,
                         const char* string);
LIBPKG_CONFIG_SYMEXPORT void
pkg_config_fragment_copy (const pkg_config_client_t* client,
                          pkg_config_list_t* list,
                          const pkg_config_fragment_t* base,
                          bool is_private);
LIBPKG_CONFIG_SYMEXPORT void
pkg_config_fragment_copy_list (const pkg_config_client_t* client,
                               pkg_config_list_t* list,
                               const pkg_config_list_t* base);
LIBPKG_CONFIG_SYMEXPORT void
pkg_config_fragment_delete (pkg_config_list_t* list,
                            pkg_config_fragment_t* node);
LIBPKG_CONFIG_SYMEXPORT void
pkg_config_fragment_free (pkg_config_list_t* list);
LIBPKG_CONFIG_SYMEXPORT void
pkg_config_fragment_filter (const pkg_config_client_t* client,
                            pkg_config_list_t* dest,
                            pkg_config_list_t* src,
                            pkg_config_fragment_filter_func_t filter_func,
                            void* data);
LIBPKG_CONFIG_SYMEXPORT size_t
pkg_config_fragment_render_len (const pkg_config_list_t* list,
                                bool escape,
                                const pkg_config_fragment_render_ops_t* ops);
LIBPKG_CONFIG_SYMEXPORT void
pkg_config_fragment_render_buf (const pkg_config_list_t* list,
                                char* buf,
                                size_t len,
                                bool escape,
                                const pkg_config_fragment_render_ops_t* ops);
LIBPKG_CONFIG_SYMEXPORT char*
pkg_config_fragment_render (const pkg_config_list_t* list,
                            bool escape,
                            const pkg_config_fragment_render_ops_t* ops);
LIBPKG_CONFIG_SYMEXPORT bool
pkg_config_fragment_has_system_dir (const pkg_config_client_t* client,
                                    const pkg_config_fragment_t* frag);

/* fileio.c */
LIBPKG_CONFIG_SYMEXPORT char*
pkg_config_fgetline (char* line, size_t size, FILE* stream);

/* tuple.c */
LIBPKG_CONFIG_SYMEXPORT pkg_config_tuple_t*
pkg_config_tuple_add (const pkg_config_client_t* client,
                      pkg_config_list_t* parent,
                      const char* key,
                      const char* value,
                      bool parse);
LIBPKG_CONFIG_SYMEXPORT char*
pkg_config_tuple_find (const pkg_config_client_t* client,
                       pkg_config_list_t* list,
                       const char* key);
LIBPKG_CONFIG_SYMEXPORT char*
pkg_config_tuple_parse (const pkg_config_client_t* client,
                        pkg_config_list_t* list,
                        const char* value);
LIBPKG_CONFIG_SYMEXPORT void
pkg_config_tuple_free (pkg_config_list_t* list);
LIBPKG_CONFIG_SYMEXPORT void
pkg_config_tuple_free_entry (pkg_config_tuple_t* tuple,
                             pkg_config_list_t* list);
LIBPKG_CONFIG_SYMEXPORT void
pkg_config_tuple_add_global (pkg_config_client_t* client,
                             const char* key,
                             const char* value);
LIBPKG_CONFIG_SYMEXPORT char*
pkg_config_tuple_find_global (const pkg_config_client_t* client,
                              const char* key);
LIBPKG_CONFIG_SYMEXPORT void
pkg_config_tuple_free_global (pkg_config_client_t* client);
LIBPKG_CONFIG_SYMEXPORT void
pkg_config_tuple_define_global (pkg_config_client_t* client, const char* kv);

/* cache.c */
LIBPKG_CONFIG_SYMEXPORT pkg_config_pkg_t*
pkg_config_cache_lookup (pkg_config_client_t* client, const char* id);
LIBPKG_CONFIG_SYMEXPORT void
pkg_config_cache_add (pkg_config_client_t* client, pkg_config_pkg_t* pkg);
LIBPKG_CONFIG_SYMEXPORT void
pkg_config_cache_remove (pkg_config_client_t* client, pkg_config_pkg_t* pkg);
LIBPKG_CONFIG_SYMEXPORT void
pkg_config_cache_free (pkg_config_client_t* client);

/* path.c */
LIBPKG_CONFIG_SYMEXPORT void
pkg_config_path_add (const char* text, pkg_config_list_t* dirlist, bool filter);
LIBPKG_CONFIG_SYMEXPORT size_t
pkg_config_path_split (const char* text,
                       pkg_config_list_t* dirlist,
                       bool filter);
LIBPKG_CONFIG_SYMEXPORT size_t
pkg_config_path_build_from_environ (const char* envvarname,
                                    const char* fallback,
                                    pkg_config_list_t* dirlist,
                                    bool filter);
LIBPKG_CONFIG_SYMEXPORT bool
pkg_config_path_match_list (const char* path, const pkg_config_list_t* dirlist);
LIBPKG_CONFIG_SYMEXPORT void
pkg_config_path_free (pkg_config_list_t* dirlist);
LIBPKG_CONFIG_SYMEXPORT bool
pkg_config_path_relocate (char* buf, size_t buflen);
LIBPKG_CONFIG_SYMEXPORT void
pkg_config_path_copy_list (pkg_config_list_t* dst,
                           const pkg_config_list_t* src);

#ifdef __cplusplus
}
#endif

#endif /* LIBPKG_CONFIG_PKG_CONFIG_H */
