# todo

A to-do list with a GTK 4 window, kept in an SQLite database.

```sh
molto run     # open the window
molto test    # the store, against a real SQLite
```

Both libraries come from the molto registry, and neither is installed
system-wide:

```toml
[deps]
gtk = "4.14.0"      # a platform recipe: each platform's own GTK packages
sqlite = "3.53.4"   # a source recipe: the amalgamation, compiled as part of the build
```

## What it does

- Add a task with Enter or **Add**; tick it off, double-click its title to edit
  it, or delete it with **✕**.
- Filter by **All**, **Active** or **Done**; **Clear done** removes every
  finished task.
- The list is saved as you go, in `todos.db` under the user's data directory
  (`~/.local/share/molto-todo` on Linux, `~/Library/Application Support/molto-todo`
  on macOS). `TODO_DB=/some/file.db molto run` uses another file.

## Layout

| Path | What |
|---|---|
| `src/store.c`, `include/todo/store.h` | The list in SQLite: schema with a version, bound parameters, one statement per change |
| `src/main.c` | The GTK window; every change goes to the store and the list is redrawn from it |
| `tests/test_store.c` | moltest suite for the store, in memory and on disk |
