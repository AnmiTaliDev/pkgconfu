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
dependencies), matching pkgconf's traversal: `Requires.private` before
`Requires`, entries walked in reverse. Deduplication follows pkgconf: `-I` and
`-L` keep the first occurrence, `-l` and `-pthread` move to the last. Inline
`#` comments and `\` line continuations in `.pc` files are handled.

## Sysroot

With `PKG_CONFIG_SYSROOT_DIR` set, `-I` and `-L` paths in the output are
prefixed with the sysroot, and the `pc_sysrootdir` variable is set accordingly.

## Uninstalled packages

A `module-uninstalled.pc` file in the search path takes precedence over
`module.pc`, unless `PKG_CONFIG_DISABLE_UNINSTALLED` is set.

## Provides

If a requested module name has no matching `.pc` file, package files are scanned
for a `Provides` entry with that name.

## Relocation

`--define-prefix` sets each package's `prefix` variable to the directory two
levels above its `.pc` file. `--prefix-variable=NAME` changes which variable is
redefined.

## Symlinked package files

When a `.pc` file is a symlink, `pcfiledir` is computed from the link target
joined to the link's directory, without canonicalisation, matching pkgconf.

## Drop-in use

`make install PKG_CONFIG_SYMLINK=1` additionally installs a `pkg-config`
symlink. Build systems that shell out to `pkg-config` then use pkgconfu.

## Scope

Implemented:

- `.pc` parsing, variable substitution, `--define-variable`
- `Requires`, `Requires.private`, `Conflicts`, `Cflags.private`, `Provides`
- `--cflags`, `--libs` and their filtered variants, `--static`, `--msvc-syntax`
- `--modversion`, `--exists`, `--variable`, `--print-variables`
- `--print-requires`, `--print-requires-private`, `--print-provides`
- `--validate`, `--path`, `--list-all`
- version constraints and `--atleast-version` / `--exact-version` /
  `--max-version` / `--atleast-pkgconfig-version`
- `--with-path`, `--maximum-traverse-depth`, `--keep-system-cflags`,
  `--keep-system-libs`, `--define-prefix`, `--prefix-variable`
- `PKG_CONFIG_SYSROOT_DIR`, `PKG_CONFIG_SYSTEM_INCLUDE_PATH`,
  `PKG_CONFIG_SYSTEM_LIBRARY_PATH`, `PKG_CONFIG_DISABLE_UNINSTALLED`
- `*-uninstalled.pc` files and `.pc` files reached through symlinks
- `pkg-config` and `pkgconf` as built-in virtual packages
- pkg-config compatible error message wording

Not yet implemented, planned for later releases:

- `Provides` version ranges and package renames
- resolving `.pc` symlinks when computing `pcfiledir`
- full `pc_top_builddir` build-tree handling

## Tests

`make check` runs `tests/run.sh` against a set of fixture `.pc` files.
