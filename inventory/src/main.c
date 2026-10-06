/* Keeps a small inventory in PostgreSQL and exports it as XML.
 *
 *   inventory "host=localhost user=postgres"
 *
 * Creates the table if it is missing, replaces its rows with a fixed stock,
 * reads them back in order with a parameterised query, and writes them to
 * standard output as an ISO-8859-1 XML document. */

#include <libpq-fe.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    const char *name;
    const char *quantity;
} item;

static const item STOCK[] = {
    {"caf\xc3\xa9 en grano", "12"},
    {"t\xc3\xa9 verde", "30"},
    {"az\xc3\xba" "car", "7"},
};

static int fail(PGconn *conn, PGresult *res, const char *what) {
    fprintf(stderr, "inventory: %s: %s", what, PQerrorMessage(conn));
    PQclear(res);
    PQfinish(conn);
    return 1;
}

static int command(PGconn *conn, const char *sql) {
    PGresult *res = PQexec(conn, sql);
    if(PQresultStatus(res) != PGRES_COMMAND_OK)
        return fail(conn, res, sql);
    PQclear(res);
    return 0;
}

static int store(PGconn *conn) {
    if(command(conn, "CREATE TABLE IF NOT EXISTS items (name text PRIMARY KEY, quantity int)") ||
       command(conn, "TRUNCATE items"))
        return 1;
    for(size_t i = 0; i < sizeof STOCK / sizeof STOCK[0]; i++) {
        const char *params[] = {STOCK[i].name, STOCK[i].quantity};
        PGresult *res = PQexecParams(conn, "INSERT INTO items VALUES ($1, $2::int)", 2, NULL,
                                     params, NULL, NULL, 0);
        if(PQresultStatus(res) != PGRES_COMMAND_OK)
            return fail(conn, res, "INSERT");
        PQclear(res);
    }
    return 0;
}

static xmlDocPtr export_items(PGresult *res) {
    xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
    xmlNodePtr root = xmlNewNode(NULL, BAD_CAST "inventory");
    xmlDocSetRootElement(doc, root);
    for(int row = 0; row < PQntuples(res); row++) {
        xmlNodePtr node = xmlNewTextChild(root, NULL, BAD_CAST "item",
                                          BAD_CAST PQgetvalue(res, row, 0));
        xmlNewProp(node, BAD_CAST "quantity", BAD_CAST PQgetvalue(res, row, 1));
    }
    return doc;
}

int main(int argc, char **argv) {
    const char *conninfo = argc > 1 ? argv[1] : getenv("DATABASE_URL");
    if(conninfo == NULL) {
        fprintf(stderr, "usage: inventory <conninfo>  (or set DATABASE_URL)\n");
        return 2;
    }

    PGconn *conn = PQconnectdb(conninfo);
    if(PQstatus(conn) != CONNECTION_OK)
        return fail(conn, NULL, "connect");
    if(PQsetClientEncoding(conn, "UTF8") != 0 || store(conn))
        return 1;

    PGresult *res = PQexec(conn, "SELECT name, quantity FROM items ORDER BY name");
    if(PQresultStatus(res) != PGRES_TUPLES_OK)
        return fail(conn, res, "SELECT");

    /* libxml2 holds text as UTF-8 and converts on the way out: ISO-8859-1
       goes through iconv. */
    xmlDocPtr doc = export_items(res);
    xmlChar *xml = NULL;
    int size = 0;
    xmlDocDumpFormatMemoryEnc(doc, &xml, &size, "ISO-8859-1", 1);
    fwrite(xml, 1, (size_t)size, stdout);

    xmlFree(xml);
    xmlFreeDoc(doc);
    PQclear(res);
    PQfinish(conn);
    xmlCleanupParser();
    return 0;
}
