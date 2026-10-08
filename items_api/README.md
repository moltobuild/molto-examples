# items_api

A small items CRUD API in C, served by libwebsockets and persisted in PostgreSQL
through libpq. There is no frontend. Molto resolves the compiler and build
tools through pickup; no compiler paths or PATH overrides are needed.

## Run

Use Molto 0.64.0 or later, pickup, Docker with a running engine, and Compose.
The registry supplies libwebsockets 5.0.0, libpq 18.6.0 and OpenSSL. Their
upstream configuration uses CMake and Ninja; if pickup has neither, install
them with `pickup install cmake ninja`.

```sh
cd items_api
cp .env.example .env
docker compose up -d --wait db
molto run
```

If Compose is installed as a standalone executable, use `docker-compose`
instead of `docker compose`. The server listens on `127.0.0.1:8080`.

The application reads `.env` automatically, relative to the project directory:

```dotenv
DATABASE_URL=postgresql://items:items_dev@127.0.0.1:5432/items?connect_timeout=3
PORT=8080
```

Only `DATABASE_URL` and `PORT` are supported, as plain `KEY=value` assignments
with optional quotes. There is no shell expansion. Existing environment
values override the file. `.env` is ignored by Git; `.env.example` is the
template. The credentials above are for this local example.

Compose initializes `db/migrations/001_create_items.sql` on the first boot of
an empty volume. Data persists in the named volume when the container stops.
Schema edits do not automatically rerun on an existing volume.

## Try the API with curl

In another terminal:

```sh
# Liveness (the process is running; database errors are reported by item routes).
curl -i http://127.0.0.1:8080/health

# Create: 201, with the new item as JSON.
curl -i http://127.0.0.1:8080/items \
  -H 'Content-Type: application/json' \
  -d '{"name":"Keyboard","price":49.90,"stock":8}'

# Use the id returned by POST; it is 1 only on a fresh database.
ITEM_ID=1

# List: 200, JSON array ordered by id. Default limit=20, offset=0.
curl -i 'http://127.0.0.1:8080/items?limit=10&offset=0'

# Get one: 200; a missing id returns 404.
curl -i "http://127.0.0.1:8080/items/$ITEM_ID"

# Partial update: 200. Fields omitted from PATCH are preserved.
curl -i -X PATCH "http://127.0.0.1:8080/items/$ITEM_ID" \
  -H 'Content-Type: application/json' \
  -d '{"price":44.50,"stock":0}'

# Rename without changing price or stock.
curl -i -X PATCH "http://127.0.0.1:8080/items/$ITEM_ID" \
  -H 'Content-Type: application/json' \
  -d '{"name":"Wireless keyboard"}'

# Delete: 204, no response body. Repeating it returns 404.
curl -i -X DELETE "http://127.0.0.1:8080/items/$ITEM_ID"

# Invalid input: 422, a JSON error.
curl -i http://127.0.0.1:8080/items \
  -H 'Content-Type: application/json' \
  -d '{"name":"Keyboard","price":-1,"stock":8}'
```

Every item has `id`, `name`, `price`, `stock`, `updated_at` and `created_at`.
IDs are positive PostgreSQL bigints. Names are 1-120 UTF-8 bytes, must contain
non-whitespace text and cannot contain control characters. Price is an exact
`numeric(12,2)` from 0 to 9999999999.99; JSON requests use ordinary numbers with
at most two decimal places, without exponent notation. Stock is an integer
from 0 to 2147483647. POST requires all three writable fields; PATCH requires
at least one. Unknown, duplicate, null and read-only fields are rejected.

PostgreSQL generates IDs and UTC timestamps. A trigger preserves `created_at`
and refreshes `updated_at` whenever a row is updated. Queries use bound
parameters, and PostgreSQL encodes response JSON, including escaped names.

| Route | Method | Success |
|---|---|---|
| `/items` | POST | 201, created item |
| `/items?limit=20&offset=0` | GET | 200, array (limit 1-100, offset nonnegative) |
| `/items/{id}` | GET | 200, item |
| `/items/{id}` | PATCH | 200, updated item |
| `/items/{id}` | DELETE | 204, empty body |

Errors use `{"error":"..."}`: 400 for malformed JSON, invalid IDs or query
parameters; 404 for missing items or routes; 405 with `Allow` for unsupported
methods; 411 for writes without Content-Length; 413 for bodies over 4096 bytes;
415 for writes without `application/json`; 422 for invalid fields; 503 when the
database connection is unavailable; 500 for a failed database operation.

## Tests

```sh
molto test
molto test --profile coverage
molto build --profile release

# Optional live check, while molto run is running and the database is up.
python3 scripts/smoke.py
```

Unit tests run without Docker or a database. Moltest checks validation and
configuration; moltest-mock replaces repository results for routes and
`db_query` for SQL tests. `[[test.isolated]]` keeps each mock in its own binary.
There is no SQLite backend and no TEST_DATABASE_URL. Moltest-coverage produces
`build/coverage.lcov` and `build/coverage.json`. The plugin releases currently
pin moltest to an immutable migrated revision, so this example selects the same
revision to keep one runner in the dependency graph.

The optional live check exercises PostgreSQL persistence, escaping, partial
updates, timestamp preservation, pagination and deletion over HTTP. It creates
and deletes its own item, leaving other rows intact. The example uses synchronous
libpq on one event loop for readability; it is a local development scaffold,
with no authentication or connection pool.

## Layout

```text
items_api/
├── Project.toml
├── .env.example
├── docker-compose.yml
├── db/migrations/001_create_items.sql
├── include/items_api/
│   ├── config/env.h
│   ├── db/database.h
│   ├── models/item.h
│   ├── repositories/items.h
│   └── routes/items.h
├── src/
│   ├── main.c                 # libwebsockets transport and lifecycle
│   ├── config/env.c           # bounded .env loading
│   ├── db/database.c          # libpq connection and query execution
│   ├── models/item.c          # JSON input, UTF-8 and value validation
│   ├── repositories/items.c   # parameterized SQL
│   └── routes/items.c         # HTTP routing and response mapping
├── tests/{config,db,models,repositories,routes}/
└── scripts/smoke.py           # optional live PostgreSQL API check
```

Stop the database with `docker compose down`; this retains its data.
To deliberately erase this example's database and apply the migration again,
use `docker compose down -v`, then `docker compose up -d --wait db`.

API references: [libwebsockets HTTP callbacks](https://libwebsockets.org/lws-api-doc-main/html/group__usercb.html),
[libpq parameterized commands](https://www.postgresql.org/docs/18/libpq-exec.html),
[PostgreSQL Docker image](https://hub.docker.com/_/postgres).
