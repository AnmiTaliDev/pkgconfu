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
pkgconfu --print-requires gio-2.0
pkgconfu --validate ./mylib.pc
pkgconfu --list-all
```

## Search path

The search path is built from `--with-path` arguments, then `PKG_CONFIG_PATH`,
then the default directories. `PKG_CONFIG_LIBDIR` replaces the default
directories. The compiled-in default is set by `PKGCONFU_DEFAULT_PATH` at build
time.

## Output filtering

- `--cflags-only-I`, `--cflags-only-other`
- `--libs-only-l`, `--libs-only-L`, `--libs-only-other`

`-I/usr/include` is removed from compile output and `-L/usr/lib`,
`-L/usr/lib64` from link output, unless `--keep-system-cflags` /
`--keep-system-libs` or the `PKG_CONFIG_ALLOW_SYSTEM_CFLAGS` /
`PKG_CONFIG_ALLOW_SYSTEM_LIBS` environment variables are set.

## Dependency resolution

`Requires` and `Requires.private` are resolved transitively. For `--cflags`,
flags from both are included. For `--libs`, private requirements contribute only
under `--static`. `Conflicts` is enforced across the resolved set.
`--maximum-traverse-depth=N` limits recursion.

Output is emitted in a topological order (each package before its
dependencies). This order is not yet byte-for-byte identical to pkg-config for
every package graph.

## Sysroot

With `PKG_CONFIG_SYSROOT_DIR` set, `-I` and `-L` paths in the output are
prefixed with the sysroot, and the `pc_sysrootdir` variable is set accordingly.

## Scope

Implemented:

- `.pc` parsing, variable substitution, `--define-variable`
- `Requires`, `Requires.private`, `Conflicts`, `Cflags.private`
- `--cflags`, `--libs` and their filtered variants, `--static`
- `--modversion`, `--exists`, `--variable`, `--print-variables`
- `--print-requires`, `--print-requires-private`, `--print-provides`
- `--validate`, `--list-all`
- version constraints and `--atleast-version` / `--exact-version` /
  `--max-version` / `--atleast-pkgconfig-version`
- `--with-path`, `--maximum-traverse-depth`, `--keep-system-cflags`,
  `--keep-system-libs`
- `PKG_CONFIG_SYSROOT_DIR` prefixing and `pc_sysrootdir`

Not yet implemented, planned for later releases:

- byte-for-byte flag ordering and deduplication identical to pkg-config
- uninstalled package files (`*-uninstalled.pc`)
- `Provides` based module name resolution and renames
- `--msvc-syntax`, `--define-prefix`
- full `pc_top_builddir` build-tree handling

## Tests

`make check` runs `tests/run.sh` against a set of fixture `.pc` files.
