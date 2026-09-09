# Contributing

## Reporting issues

Open an issue with:

- the `pkgconfu` version (`pkgconfu --version`)
- the exact command line
- the `.pc` files involved, or a minimal reproduction
- what you expected and what happened, including output from the reference
  `pkg-config` where relevant

## Building for development

```
make CFLAGS="-O1 -g -fsanitize=address,undefined" \
     LDFLAGS="-fsanitize=address,undefined"
```

The code targets C23 and builds cleanly with `-Wall -Wextra`.

## Patches

- Keep changes focused; one logical change per commit.
- Match the existing code style.
- Describe how you tested the change. Comparing output against the reference
  `pkg-config` for a range of real `.pc` files is the most useful check.
- Commit messages follow Conventional Commits.

## Compatibility work

Behavioral differences from pkg-config are tracked as compatibility gaps. When
closing one, add or extend a test case that pins the expected output.
