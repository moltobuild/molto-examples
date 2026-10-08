#ifndef ITEMS_API_DB_DATABASE_H
#define ITEMS_API_DB_DATABASE_H
#include <libpq-fe.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum { DB_OK, DB_NOT_FOUND, DB_UNAVAILABLE, DB_ERROR } db_status;
PGconn *db_open(const char *url);
void db_close(PGconn *conn);
/* One parameterized statement returning one JSON text cell, or no rows. */
db_status db_query(PGconn *conn, const char *sql, int count, const char *const *values, char *json,
                   size_t capacity);
#endif
