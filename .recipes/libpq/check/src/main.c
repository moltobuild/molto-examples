/* Exercises libpq without a server: its version, connection-string parsing,
   escaping, and a connection attempt that must fail cleanly. With
   LIBPQ_CHECK_CONNINFO set, also runs a query against that server. */

#include <libpq-fe.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int check_conninfo(void) {
    char *err = NULL;
    PQconninfoOption *opts =
        PQconninfoParse("host=db.example port=6543 dbname=shop sslmode=require", &err);
    if(opts == NULL) {
        fprintf(stderr, "PQconninfoParse failed: %s\n", err != NULL ? err : "?");
        PQfreemem(err);
        return 0;
    }
    int seen = 0;
    for(PQconninfoOption *o = opts; o->keyword != NULL; o++) {
        if(o->val == NULL)
            continue;
        if((strcmp(o->keyword, "host") == 0 && strcmp(o->val, "db.example") == 0) ||
           (strcmp(o->keyword, "port") == 0 && strcmp(o->val, "6543") == 0) ||
           (strcmp(o->keyword, "sslmode") == 0 && strcmp(o->val, "require") == 0))
            seen++;
    }
    PQconninfoFree(opts);
    if(seen != 3)
        fprintf(stderr, "PQconninfoParse lost a keyword\n");
    return seen == 3;
}

static int check_unreachable(void) {
    /* Nothing listens on port 1 of the loopback address; the attempt must end
       in CONNECTION_BAD with a message, not a crash or a hang. */
    PGconn *conn = PQconnectdb("host=127.0.0.1 port=1 connect_timeout=5 sslmode=disable");
    int ok = conn != NULL && PQstatus(conn) == CONNECTION_BAD && PQerrorMessage(conn)[0] != '\0';
    if(!ok)
        fprintf(stderr, "an unreachable server did not fail cleanly\n");
    PQfinish(conn);
    return ok;
}

static int check_ssl_built_in(void) {
    /* sslmode=require must be accepted, which a libpq without TLS refuses. */
    PGconn *conn = PQconnectdb("host=127.0.0.1 port=1 connect_timeout=5 sslmode=require");
    int ok = conn != NULL && strstr(PQerrorMessage(conn), "not supported") == NULL;
    if(!ok)
        fprintf(stderr, "libpq was built without SSL support: %s\n",
                conn != NULL ? PQerrorMessage(conn) : "");
    PQfinish(conn);
    return ok;
}

static int check_server(const char *conninfo) {
    PGconn *conn = PQconnectdb(conninfo);
    if(PQstatus(conn) != CONNECTION_OK) {
        fprintf(stderr, "could not connect: %s", PQerrorMessage(conn));
        PQfinish(conn);
        return 0;
    }
    PGresult *res = PQexec(conn, "SELECT 6 * 7, version()");
    int ok = PQresultStatus(res) == PGRES_TUPLES_OK && strcmp(PQgetvalue(res, 0, 0), "42") == 0;
    if(ok)
        printf("server: %s\n", PQgetvalue(res, 0, 1));
    else
        fprintf(stderr, "SELECT 6 * 7 did not return 42: %s", PQerrorMessage(conn));
    PQclear(res);
    PQfinish(conn);
    return ok;
}

int main(void) {
    int version = PQlibVersion();
    if(version / 10000 != 18) {
        fprintf(stderr, "PQlibVersion is %d, not 18\n", version);
        return 1;
    }
    if(!check_conninfo() || !check_unreachable() || !check_ssl_built_in())
        return 1;
    const char *conninfo = getenv("LIBPQ_CHECK_CONNINFO");
    if(conninfo != NULL && !check_server(conninfo))
        return 1;
    printf("libpq %d.%d with OpenSSL: ok\n", version / 10000, version % 10000);
    return 0;
}
