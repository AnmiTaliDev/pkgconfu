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

Version 0.1.0 implements the common subset of the pkg-config interface:
`.pc` parsing and variable substitution, `Requires` and `Requires.private`
resolution, `--cflags` and `--libs` with their filtered variants,
`--modversion`, `--exists`, `--variable`, `--print-variables`, `--list-all`,
version constraints, and system flag stripping.

Not yet covered: `PKG_CONFIG_SYSROOT_DIR` path rewriting, `Conflicts`
enforcement, `--print-requires` and related introspection options, uninstalled
package handling, and byte-for-byte flag ordering identical to pkg-config.
These are planned for later releases.

## Acknowledgments

pkgconfu follows the file format, command line interface, and behavior of
pkg-config, originally developed under the freedesktop.org project, and is
distributed under the same license.

## Documentation and contributing

- [docs/usage.md](docs/usage.md)
- [CONTRIBUTING.md](CONTRIBUTING.md)

## License

GPL-2.0-or-later. See [LICENSE](LICENSE).
