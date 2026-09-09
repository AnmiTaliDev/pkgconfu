#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
set -u

BIN=${1:-./pkgconfu}
HERE=$(CDPATH= cd "$(dirname "$0")" && pwd)
FIX=$HERE/fixtures

if [ ! -x "$BIN" ]; then
	echo "test runner: '$BIN' is not executable" >&2
	exit 2
fi
BIN=$(CDPATH= cd "$(dirname "$BIN")" && pwd)/$(basename "$BIN")

export PKG_CONFIG_LIBDIR=$FIX
unset PKG_CONFIG_PATH
unset PKG_CONFIG_SYSROOT_DIR
unset PKG_CONFIG_ALLOW_SYSTEM_CFLAGS
unset PKG_CONFIG_ALLOW_SYSTEM_LIBS
unset PKG_CONFIG_TOP_BUILD_DIR

pass=0
fail=0

out_is() {
	desc=$1
	expected=$2
	shift 2
	actual=$("$@" 2>/dev/null)
	if [ "$actual" = "$expected" ]; then
		pass=$((pass + 1))
	else
		fail=$((fail + 1))
		printf 'FAIL: %s\n  cmd:      %s\n  expected: %s\n  actual:   %s\n' \
			"$desc" "$*" "$expected" "$actual"
	fi
}

rc_is() {
	desc=$1
	expected=$2
	shift 2
	"$@" >/dev/null 2>&1
	actual=$?
	if [ "$actual" = "$expected" ]; then
		pass=$((pass + 1))
	else
		fail=$((fail + 1))
		printf 'FAIL: %s\n  cmd:      %s\n  expected rc: %s\n  actual rc:   %s\n' \
			"$desc" "$*" "$expected" "$actual"
	fi
}

out_is "version" "0.2.0" "$BIN" --version
out_is "modversion" "1.4.2" "$BIN" --modversion foo
out_is "modversion multi" "1.4.2
2.3.0" "$BIN" --modversion foo bar

out_is "cflags with dep" "-I/opt/foo/include -I/opt/bar/include" \
	"$BIN" --cflags foo
out_is "cflags static adds Cflags.private" \
	"-I/opt/foo/include -I/opt/bar/include -DBAR_STATIC" \
	"$BIN" --static --cflags foo

out_is "libs with dep" "-L/opt/foo/lib -lfoo -L/opt/bar/lib -lbar" \
	"$BIN" --libs foo
out_is "libs static pulls private deps" \
	"-L/opt/foo/lib -lfoo -L/opt/bar/lib -lbar -lm -L/opt/baz/lib -lbaz" \
	"$BIN" --static --libs foo

out_is "libs-only-l" "-lfoo -lbar" "$BIN" --libs-only-l foo
out_is "libs-only-L" "-L/opt/foo/lib -L/opt/bar/lib" "$BIN" --libs-only-L foo
out_is "cflags-only-other empty" "" "$BIN" --cflags-only-other foo

out_is "variable" "/opt/foo/lib" "$BIN" --variable=libdir foo
out_is "define-variable override applies globally" "-I/CUSTOM/include" \
	"$BIN" --define-variable=prefix=/CUSTOM --cflags foo
out_is "print-requires" "bar >= 2.0" "$BIN" --print-requires foo
out_is "print-requires-private" "baz" "$BIN" --print-requires-private bar
out_is "print-provides" "foo = 1.4.2" "$BIN" --print-provides foo

rc_is "exists ok" 0 "$BIN" --exists foo
rc_is "exists missing" 1 "$BIN" --exists nope
rc_is "exists version ok" 0 "$BIN" --exists "foo >= 1.0"
rc_is "exists version fail" 1 "$BIN" --exists "foo >= 9.0"
rc_is "atleast-version" 0 "$BIN" --atleast-version=1.4.2 foo
rc_is "exact-version fail" 1 "$BIN" --exact-version=1.0.0 foo
rc_is "max-version" 0 "$BIN" --max-version=2.0.0 foo

rc_is "conflicts detected" 1 "$BIN" --exists needsboth
rc_is "conflicter alone ok" 0 "$BIN" --exists conflicter
rc_is "cycle terminates" 0 "$BIN" --exists cycle-a
out_is "cycle libs" "-lcycle-a -lcycle-b" "$BIN" --libs cycle-a

rc_is "validate ok" 0 "$BIN" --validate foo
rc_is "validate broken" 1 "$BIN" --validate broken

out_is "max-traverse-depth 1 skips deps" "-I/opt/foo/include" \
	"$BIN" --maximum-traverse-depth=1 --cflags foo

out_is "keep-system-cflags" "-I/usr/include/sysroot" \
	"$BIN" --cflags sysroot-lib
out_is "sysroot rewrites -I and -L" \
	"-I/SYS/usr/include/sysroot -L/SYS/usr/lib -lsysroot" \
	env PKG_CONFIG_SYSROOT_DIR=/SYS "$BIN" --cflags --libs sysroot-lib
out_is "pc_sysrootdir variable" "/SYS" \
	env PKG_CONFIG_SYSROOT_DIR=/SYS "$BIN" --variable=pc_sysrootdir foo

rc_is "atleast-pkgconfig-version ok" 0 "$BIN" --atleast-pkgconfig-version=0.29
rc_is "atleast-pkgconfig-version fail" 1 "$BIN" --atleast-pkgconfig-version=1.0

printf '\n%d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]
