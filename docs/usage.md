# Usage

`pkgconfu` reads pkg-config metadata files (`.pc`) and prints the flags needed
to compile and link against the named libraries.

## Package arguments

A package argument is a module name, optionally followed by a version
constraint:

```
pkgconfu --cflags "glib-2.0 >= 2.40"
pkgconfu --libs zlib libpng
```

Operators: `<`, `<=`, `=`, `!=`, `>=`, `>`.

## Common invocations

```
pkgconfu --modversion zlib
pkgconfu --cflags --libs glib-2.0
pkgconfu --libs --static libfoo
pkgconfu --variable=prefix zlib
pkgconfu --exists "libcurl >= 7.60" && echo present
pkgconfu --list-all
```

## Search path

The search path is built from `PKG_CONFIG_PATH` (prepended) followed by the
default directories. `PKG_CONFIG_LIBDIR` replaces the default directories. The
compiled-in default is set by `PKGCONFU_DEFAULT_PATH` at build time.

## Output filtering

- `--cflags-only-I`, `--cflags-only-other`
- `--libs-only-l`, `--libs-only-L`, `--libs-only-other`

`-I/usr/include` is removed from compile output and `-L/usr/lib`,
`-L/usr/lib64` from link output, unless `PKG_CONFIG_ALLOW_SYSTEM_CFLAGS` or
`PKG_CONFIG_ALLOW_SYSTEM_LIBS` is set.

## Scope of 0.1.0

Implemented:

- `.pc` parsing, variable substitution, `--define-variable`
- `Requires` and `Requires.private` resolution
- `--cflags`, `--libs` and their filtered variants
- `--modversion`, `--exists`, `--variable`, `--print-variables`, `--list-all`
- version constraints and `--atleast-version` / `--exact-version` /
  `--max-version`
- system flag stripping

Not yet implemented, planned for later releases:

- `PKG_CONFIG_SYSROOT_DIR` and related path rewriting
- `Conflicts` enforcement
- `--print-requires`, `--print-provides`, `--validate`
- byte-for-byte flag ordering compatibility with pkg-config
- uninstalled package files and `.pc` file provides/rename handling
