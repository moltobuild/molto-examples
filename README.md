# molto examples

Small, complete projects built with [molto](https://github.com/moltobuild/molto),
the package manager and build system for C and C++. Each directory is a project
of its own: a `Project.toml`, its sources, its tests, and nothing else to set up.

| Example | What it shows |
|---|---|
| [`wordcount`](wordcount/) | A `wc` clone: a C23 executable, `[target].requires` answered by pickup, tests that bring their own `main()` |
| [`calculator`](calculator/) | A GTK 4 desktop app: a dependency from the registry that downloads each platform's own GTK, and a moltest suite |

## Running one

Install molto (and [pickup](https://github.com/moltobuild/pickup), which finds a
compiler for it), then from an example's directory:

```sh
molto build   # compile
molto run     # build and run the executable
molto test    # build and run the tests
```

Nothing is installed system-wide: dependencies land in molto's cache
(`~/.cache/molto`) and are shared by every project on the machine.

## The examples

### wordcount

Counts lines, words and bytes, like `wc`, from files or standard input.

- **No dependencies.** The whole manifest is a package, a `[target]` and two
  profiles.
- **`requires = ["attr_nodiscard"]`** names a property of the code — C23's
  `[[nodiscard]]` — and pickup chooses a local compiler that provides it. The
  manifest never names a compiler binary.
- **Tests in `per_file` mode**, molto's default: each file in `tests/` is its own
  executable, linked against `src/` minus `main.c`, and reports through its exit
  status. No test framework needed.
- `MOLTO_PKG_NAME` and `MOLTO_PKG_VERSION` reach the code from the manifest,
  which is what `wordcount --version` prints.

```sh
cd wordcount
molto run -- src/count.c src/main.c
```

### calculator

A calculator with a GTK 4 window: precedence (`2+3×4` is 14), parentheses,
`%`, a live preview of the result, and keyboard input.

- **`gtk = "4.14.0"`** comes from the molto registry as a *platform recipe*
  ([RFC-0022](https://github.com/moltobuild/molto/blob/master/rfcs/0022-platform-packages.md)):
  molto uses the system's GTK when it has one with its headers, and otherwise
  downloads that platform's own packages — Homebrew bottles on macOS, MSYS2 on
  Windows, the distribution's development packages on Ubuntu, Fedora and Arch —
  verifies every file's sha256 and unpacks them into its cache. No package
  manager runs.
- **The logic is separate from the window.** `src/calc.c` is plain C and is what
  the tests exercise; `src/main.c` is only the GTK interface.
- **Tests with moltest** in `single` mode, taken as a `[dev-deps]` entry: the
  framework owns `main()` and never reaches the shipped executable.

```sh
cd calculator
molto run
```

## Adding an example

Start one with `molto new --bin <name>` (or `--lib` for a library tested with
moltest), keep it small enough to read in one sitting, and give it a line in the
table above saying what it shows that the others do not.
