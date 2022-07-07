# libpkg-config - retrieve compiler/linker flags and other metadata

The `libpkg-config` library provides a C API for retrieving C-language family
compiler/linker flags and other metadata from `pkg-config` files (`.pc`). It's
primarily aimed at build system and other similar tools (see the recommended
usage below).

This library is a fork of `libpkgconf` from the [`pkgconf`][pkgconf] project.
The fork was motivated by the poor quality of changes in general (see these
three commits for a representative example: [`0253fdd`][0253fdd]
[`c613eb5`][c613eb5] [`ab404bc`][ab404bc]) and disregard for backwards
compatibility in particular (for example, [sr.ht/18][srht18]). While we have
removed a large amount of non-essential functionality and "innovations" as
well as spend some time fixing and cleaning things up, make no mistake it is
still an over-engineered, inscrutable mess and there likely are bugs in the
corner cases. However, we do use it in [`build2`][build2], which means at
least the recommended usage patterns (see below) are reasonably well
tested. We hope to continue cleaning things up as time permits but a complete
require is also on the cards (in which case it will most likely become a C++
library).


## Recommended usage

Traditionally, `pkg-config` is used like this: instead of just passing `-lfoo`
during linking, we call `pkg-config --cflags foo` passing the returned options
during compilation and then call `pkg-config --libs foo` passing the returned
options and libraries during linking. In other words, instead of letting the
compiler find `libfoo.a` or `libfoo.so` (or their equivalents on other
platforms) using its library search paths, we rather let `pkg-config` find
`foo.pc` using its `pkg-config` library search paths.

This works reasonably well if the two sets of search paths are consistent,
which is normally the case for the native compilation using standard
installation locations. For example, if our C/C++ toolchain is configured to
look for libraries in `/usr/local/lib` and `/usr/lib` and our `pkg-config` is
configured to look for `.pc` files in `/usr/local/lib/pkgconfig` and
`/usr/lib/pkgconfig`, then everything will most likely work smoothly.

However, if these two sets of search paths start diverging, for example due to
custom installation locations, cross-compilation, etc., then things will most
likely not go smoothly at ll. The traditional `pkg-config` answer to this type
of problems is an array of environment variables to tweak various aspects of
the search paths, the `sysroot` rewrite logic, and, in case of `pkgconf`, the
cross-personality functionality.

However, an astute reader will undoubtedly notice that at the root of our
troubles are the two sets of search paths that have the inevitable tendency to
become inconsistent (as all search paths tend to do). And if only we could go
back to the one set, the single source of truth, then all our troubles will
likely disappear and we won't need an impressive list of hacks and
workarounds.

We've decided to use this approach in `build2`. As a step one, we collect
exactly the same library search paths as what will be used by the compiler.
Specifically, we parse the linker options specified by the user and collect
all the paths specified with `-L`. Then we extract the system library search
paths by running the compiler with `-print-search-dirs` (this option is
supported by both GCC and Clang).

Next, from this list of library search paths we derive a parallel list of the
`.pc` file search paths. On most platforms this is a matter of just appending
the `pkgconfig` subdirectory to each library search path (of course, someone
had to be different and in this case it is FreeBSD).

Finally, we initialize the `libpkg-config`'s `pkg_config_client::dir_list`
only with these `.pc` file search paths omitting all the built-in paths and
environment variables.

(To be precise, in `build2` we go a step further and search for the library
ourselves, then locate the corresponding `.pc` file, if any, and load it
directly.)

You can find the complete source code that implements this approach with the
help of `libpkg-config` in [`pkgconfig.hxx`][pkgconfig.hxx] and
[`pkgconfig-libpkg-config.cxx`][pkgconfig-libpkg-config.hxx].


[pkgconf]: https://github.com/pkgconf/pkgconf

[0253fdd]: https://github.com/pkgconf/pkgconf/commit/0253fddc1d64
[c613eb5]: https://github.com/pkgconf/pkgconf/commit/c613eb5ccee2
[ab404bc]: https://github.com/pkgconf/pkgconf/commit/ab404bc25b94

[srht18]: https://todo.sr.ht/~kaniini/pkgconf/18

[build2]: https://build2.org

[pkgconfig.hxx]: https://github.com/build2/build2/blob/master/libbuild2/cc/pkgconfig.hxx
[pkgconfig-libpkg-config.hxx]: https://github.com/build2/build2/blob/master/libbuild2/cc/pkgconfig-libpkg-config.cxx
