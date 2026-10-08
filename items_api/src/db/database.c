#include <items_api/db/database.h>
#include <stdio.h>
#include <string.h>

PGconn *db_open(const char *url) {
    PGconn *conn = PQconnectdb(url);
    if(!conn || PQstatus(conn) != CONNECTION_OK) {
        fprintf(stderr, "Could not connect to PostgreSQL\n");
        if(conn)
            PQfinish(conn);
        return NULL;
    }
    PGresult *result = PQexec(conn, "SET TIME ZONE 'UTC'; SET statement_timeout = '3s'");
    bool ok = result && PQresultStatus(result) == PGRES_COMMAND_OK;
    if(result)
        PQclear(result);
    if(!ok) {
        PQfinish(conn);
        return NULL;
    }
    return conn;
}
void db_close(PGconn *conn) {
    if(conn)
        PQfinish(conn);
}
db_status db_query(PGconn *conn, const char *sql, int count, const char *const *values, char *json,
                   size_t capacity) {
    if(json && capacity)
        *json = '\0';
    if(!conn || PQstatus(conn) != CONNECTION_OK)
        return DB_UNAVAILABLE;
    PGresult *result = PQexecParams(conn, sql, count, NULL, values, NULL, NULL, 0);
    if(!result)
        return DB_UNAVAILABLE;
    db_status status = DB_OK;
    if(PQresultStatus(result) != PGRES_TUPLES_OK) {
        fprintf(stderr, "PostgreSQL query failed (SQLSTATE %s)\n",
                PQresultErrorField(result, PG_DIAG_SQLSTATE)
                    ? PQresultErrorField(result, PG_DIAG_SQLSTATE)
                    : "unknown");
        status = PQstatus(conn) == CONNECTION_OK ? DB_ERROR : DB_UNAVAILABLE;
    } else if(!PQntuples(result))
        status = DB_NOT_FOUND;
    else if(json) {
        const char *value = PQgetvalue(result, 0, 0);
        size_t length = strlen(value);
        if(length >= capacity)
            status = DB_ERROR;
        else
            memcpy(json, value, length + 1);
    }
    PQclear(result);
    return status;
}
