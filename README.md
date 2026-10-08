# molto examples

Small, complete projects built with [molto](https://github.com/moltobuild/molto),
the package manager and build system for C and C++. Each directory is a project
of its own: a `Project.toml`, its sources, its tests, and nothing else to set up.

| Example | What it shows |
|---|---|
| [`wordcount`](wordcount/) | A `wc` clone: a C23 executable, `[target].requires` answered by pickup, tests that bring their own `main()` |
| [`calculator`](calculator/) | A GTK 4 desktop app: a dependency from the registry that downloads each platform's own GTK, and a moltest suite |
| [`todo`](todo/) | Two registry dependencies at once — GTK as a platform recipe, SQLite as a source recipe — and a store tested against a real database |
| [`items_api`](items_api/) | An items CRUD API: libwebsockets, libpq, PostgreSQL in Docker Compose, route and SQL mocks, and coverage |
| [`hello_api`](hello_api/) | A REST API on libwebsockets: a source recipe that upstream's own CMake configures, with OpenSSL as its dependency |
| [`piano`](piano/) | A piano played with the mouse: SDL 3 for the window and the sound, FFmpeg to read samples and record what is played |

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

### todo

A to-do list: add, tick off, edit, delete and filter tasks, saved in SQLite as
you go.

- **Two kinds of dependency side by side.** `gtk` is a platform recipe, fetched
  as each platform's own packages; `sqlite` is a source recipe, SQLite's
  amalgamation compiled into the build with the defines its recipe pins
  (`SQLITE_THREADSAFE=1`, FTS5, R-Tree).
- **The store knows nothing of the window.** `src/store.c` is the database —
  a versioned schema, bound parameters, one statement per change — and its tests
  open it in memory and on disk.
- **The window redraws from the store** after every change, so it never shows a
  task the database does not have.

```sh
cd todo
molto run
```

### hello_api

A hello-world REST API: `GET /hello/{name}` answers `{"message":"Hello, {name}!"}`.

- **`libwebsockets = "5.0.0"`** is a source recipe configured by upstream's own
  CMake (`via = "delegate"`): molto runs it once per compiler, on this machine,
  then compiles the library itself. Its OpenSSL dependency comes along.
- **The routes know nothing of sockets.** `src/routes.c` maps a method and a
  path to a status and a JSON body, and is what the tests exercise; `src/main.c`
  is only the server.
- Configuring needs `cmake` and `ninja`: the system's if installed, otherwise
  `pickup install cmake ninja`.

```sh
cd hello_api
molto run
curl localhost:8080/hello/ana
```

### piano

A two-octave piano: click or drag across the keys, or type on the home row; R
records what you play.

- **Two large source recipes at once.** `sdl3` is configured by SDL's CMake and
  `ffmpeg` by FFmpeg's `configure`, each once per compiler on this machine,
  then molto compiles both into the build with the project's compiler.
- **FFmpeg on both sides.** `--sample FILE` decodes any audio FFmpeg reads and
  resamples it for the synth; R encodes the output to FLAC, WAV or AAC, chosen
  by extension. The tests write each format and read it back.
- **The sound knows nothing of the window.** `src/synth.c` renders voices into a
  float buffer and is tested by its pitch and fade; `src/main.c` hands it to
  SDL's audio thread.
- Configuring needs `cmake` and `ninja` for SDL (`pickup install cmake ninja`),
  and NASM for FFmpeg's assembly on x86 (`pickup install nasm`).

```sh
cd piano
molto run
```

## Adding an example

Start one with `molto new --bin <name>` (or `--lib` for a library tested with
moltest), keep it small enough to read in one sitting, and give it a line in the
table above saying what it shows that the others do not.
