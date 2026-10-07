# hello_api

A hello-world REST API: an HTTP server on libwebsockets that answers in JSON.

```sh
molto run     # serve on http://localhost:8080 (PORT=9000 molto run for another port)
molto test    # the routes, without a server
```

```sh
$ curl localhost:8080/hello/ana
{"message":"Hello, ana!"}
```

libwebsockets comes from the molto registry as a *source recipe*: upstream's
release tarball, configured once on this machine by its own CMake and compiled
into the build, with OpenSSL as its dependency.

```toml
[deps]
libwebsockets = "5.0.0"
```

Configuring it needs `cmake` and `ninja`. molto uses the system's when they are
on the PATH; otherwise `pickup install cmake ninja` fetches upstream's releases.

## The API

| Request | Status | Body |
|---|---|---|
| `GET /` or `GET /hello` | 200 | `{"message":"Hello, world!"}` |
| `GET /hello/{name}` | 200 | `{"message":"Hello, {name}!"}` |
| `GET /health` | 200 | `{"status":"ok"}` |
| a name that is not 1–32 letters, digits, `-` or `_` | 400 | `{"error":"…"}` |
| any other path | 404 | `{"error":"not found"}` |
| a method other than `GET` or `HEAD` | 405 | `{"error":"method not allowed"}` |

## Layout

| Path | What |
|---|---|
| `src/routes.c`, `include/hello_api/routes.h` | The API as a function from method and path to status and JSON; knows nothing of sockets |
| `src/main.c` | The libwebsockets server: asks `api_route` for every request and writes back what it gets |
| `tests/test_routes.c` | moltest suite for the routes |
