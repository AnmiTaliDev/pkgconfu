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

The default `.pc` search path is compiled in and can be overridden at build
time:

```
make PKGCONFU_DEFAULT_PATH=/usr/lib/pkgconfig:/usr/share/pkgconfig
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
`--validate`, `--list-all`, version constraints, `--with-path`,
`--maximum-traverse-depth`, `--keep-system-cflags` / `--keep-system-libs`,
`--define-prefix` / `--prefix-variable`, `*-uninstalled.pc` files,
`PKG_CONFIG_SYSROOT_DIR` prefixing with `pc_sysrootdir`, and configurable
system flag stripping.

Output is emitted in a valid topological order but is not yet byte-for-byte
identical to pkg-config for every package graph.

Not yet covered: exact flag ordering and deduplication parity, `Provides`
version ranges and renames, and full `pc_top_builddir` handling. These are
planned for later releases.

## Acknowledgments

pkgconfu follows the file format, command line interface, and behavior of
pkg-config, originally developed under the freedesktop.org project, and is
distributed under the same license.

## Documentation and contributing

- [docs/usage.md](docs/usage.md)
- [CONTRIBUTING.md](CONTRIBUTING.md)

## License

GPL-2.0-or-later. See [LICENSE](LICENSE).
