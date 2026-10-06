# calculator

A calculator with a GTK 4 interface, built with [molto](https://github.com/moltobuild/molto).

```sh
molto run     # open the window
molto test    # the calculator's rules, without a window
```

GTK comes from `gtk = "4.14.0"` in `Project.toml`: molto uses the system's GTK
when it has one with its headers, and otherwise downloads the platform's own
packages into `~/.cache/molto` (Homebrew bottles on macOS, MSYS2 on Windows,
the distribution's development packages on Linux).

## What it does

- Expressions with the usual precedence: `2+3×4` is 14, with parentheses,
  `%` and a leading minus.
- A live preview of the result while typing; `=` closes any open parenthesis.
- After `=`, an operator continues from the result and a digit starts over.
- Keyboard: digits, `+ - * / % ( ) .`, Enter or `=`, Backspace, Escape.

## Layout

| Path | What |
|---|---|
| `src/calc.c`, `include/calculator/calc.h` | The calculator: input rules, parser, formatting |
| `src/main.c` | The GTK window, buttons and keyboard |
| `tests/test_calc.c` | moltest suite for `calc.c` |
