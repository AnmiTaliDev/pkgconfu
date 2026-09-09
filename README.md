# pkgconfu

![license](https://img.shields.io/badge/license-GPL--2.0--or--later-blue)
![C23](https://img.shields.io/badge/C-23-blue)

## About

pkgconfu is a reimplementation of pkg-config.

## Dependencies

- A C23 compiler (GCC 14 or newer, Clang 18 or newer)
- A POSIX C library
- `make`

## Build

```
make
make check
sudo make install
```

Install locations are controlled by the usual variables:

```
make install PREFIX=/usr DESTDIR=/tmp/stage
```

`PREFIX` defaults to `/usr/local`. The binary goes to `$(PREFIX)/bin` and the
man page to `$(PREFIX)/share/man/man1`.

The default `.pc` search path and the system include/library directories are
compiled in and can be overridden at build time:

```
make PKGCONFU_DEFAULT_PATH=/usr/lib/pkgconfig:/usr/share/pkgconfig \
     PKGCONFU_SYSTEM_LIBRARY_PATH=/usr/lib:/usr/lib64
```

To also install a `pkg-config` symlink pointing at `pkgconfu`:

```
make install PKG_CONFIG_SYMLINK=1
```

## Usage

```
pkgconfu --cflags --libs glib-2.0
pkgconfu --modversion zlib
pkgconfu --exists "libcurl >= 7.60"
```

See [docs/usage.md](docs/usage.md) for the full option list and behavior.

## Compatibility

Implemented: `.pc` parsing and variable substitution, `--define-variable`,
`Requires` / `Requires.private` / `Conflicts` / `Cflags.private` / `Provides`,
`--cflags` and `--libs` with their filtered variants, `--static`,
`--msvc-syntax`, `--modversion`, `--exists`, `--variable`, `--print-variables`,
`--print-requires`, `--print-requires-private`, `--print-provides`,
`--validate`, `--path`, `--list-all`, version constraints, `--with-path`,
`--maximum-traverse-depth`, `--keep-system-cflags` / `--keep-system-libs`,
`--define-prefix` / `--prefix-variable`, `*-uninstalled.pc` files, `.pc`
symlink resolution, `pkg-config` and `pkgconf` virtual packages,
`PKG_CONFIG_SYSROOT_DIR` prefixing with `pc_sysrootdir`, and configurable
system flag stripping. Error messages match pkg-config wording.

Flag ordering and deduplication follow pkgconf. Across the package files
installed on a typical desktop system, `--cflags --libs` output matches the
system `pkg-config` byte-for-byte for about 99 percent of packages; the
remaining cases involve deduplication corner cases (`-l` after `-Wl,`
fragments, self-requiring packages).

Not yet covered: `Provides` version ranges and renames, and full
`pc_top_builddir` handling. These are planned for later releases.

## Acknowledgments

pkgconfu follows the file format, command line interface, and behavior of
pkg-config, originally developed under the freedesktop.org project, and is
distributed under the same license.

## Documentation and contributing

- [docs/usage.md](docs/usage.md)
- [CONTRIBUTING.md](CONTRIBUTING.md)

## License

GPL-2.0-or-later. See [LICENSE](LICENSE).
