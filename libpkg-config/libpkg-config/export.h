#ifndef LIBPKG_CONFIG_EXPORT_H
#define LIBPKG_CONFIG_EXPORT_H

#if defined(LIBPKG_CONFIG_STATIC)         /* Using static. */
#  define LIBPKG_CONFIG_SYMEXPORT
#elif defined(LIBPKG_CONFIG_STATIC_BUILD) /* Building static. */
#  define LIBPKG_CONFIG_SYMEXPORT
#elif defined(LIBPKG_CONFIG_SHARED)       /* Using shared. */
#  ifdef _WIN32
#    define LIBPKG_CONFIG_SYMEXPORT __declspec(dllimport)
#  else
#    define LIBPKG_CONFIG_SYMEXPORT
#  endif
#elif defined(LIBPKG_CONFIG_SHARED_BUILD) /* Building shared. */
#  ifdef _WIN32
#    define LIBPKG_CONFIG_SYMEXPORT __declspec(dllexport)
#  else
#    define LIBPKG_CONFIG_SYMEXPORT
#  endif
#else
/* If none of the above macros are defined, then we assume we are being used
 * by some third-party build system that cannot/doesn't signal the library
 * type. Note that this fallback works for both static and shared libraries
 * provided the library only exports functions (in other words, no global
 * exported data) and for the shared case the result will be sub-optimal
 * compared to having dllimport. If, however, your library does export data,
 * then you will probably want to replace the fallback with the (commented
 * out) error since it won't work for the shared case.
 */
#  define LIBPKG_CONFIG_SYMEXPORT         /* Using static or shared. */
/*#  error define LIBPKG_CONFIG_STATIC or LIBPKG_CONFIG_SHARED preprocessor macro to signal libpkg-config library type being linked */
#endif

#endif /* LIBPKG_CONFIG_EXPORT_H */
