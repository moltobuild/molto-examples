/* Exercises the parser, XPath, the iconv-backed encodings and the zlib-backed
   reader, and fails unless each gives the expected answer on this platform. */

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlsave.h>
#include <libxml/xpath.h>
#include <stdio.h>
#include <string.h>
#include <zlib.h>

static const char xml[] = "<?xml version=\"1.0\" encoding=\"ISO-8859-15\"?>\n"
                          "<menu><item price=\"3\">caf\xe9</item><item price=\"4\">\xa4</item></menu>\n";

static int check_xpath(xmlDocPtr doc) {
    xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
    xmlXPathObjectPtr sum = xmlXPathEvalExpression(BAD_CAST "sum(//item/@price)", ctx);
    int ok = sum != NULL && sum->type == XPATH_NUMBER && sum->floatval == 7.0;
    xmlXPathFreeObject(sum);
    xmlXPathFreeContext(ctx);
    if(!ok)
        fprintf(stderr, "XPath sum(//item/@price) is not 7\n");
    return ok;
}

static int check_encoding(xmlDocPtr doc) {
    /* ISO-8859-15 reaches the tree as UTF-8 only through iconv. */
    xmlChar *text = xmlNodeGetContent(xmlDocGetRootElement(doc));
    int ok = text != NULL && strcmp((const char *)text, "caf\xc3\xa9\xe2\x82\xac") == 0;
    xmlFree(text);
    if(!ok)
        fprintf(stderr, "ISO-8859-15 was not converted to UTF-8\n");
    return ok;
}

static int check_gzip(const char *path) {
    gzFile gz = gzopen(path, "wb");
    if(gz == NULL || gzwrite(gz, xml, sizeof xml - 1) != (int)(sizeof xml - 1)) {
        fprintf(stderr, "could not write %s\n", path);
        return 0;
    }
    gzclose(gz);
    xmlDocPtr doc = xmlReadFile(path, NULL, XML_PARSE_UNZIP);
    int ok = doc != NULL && check_xpath(doc);
    xmlFreeDoc(doc);
    remove(path);
    if(!ok)
        fprintf(stderr, "a gzip-compressed document was not read through zlib\n");
    return ok;
}

int main(void) {
    LIBXML_TEST_VERSION
    xmlDocPtr doc = xmlReadMemory(xml, (int)(sizeof xml - 1), "menu.xml", NULL, 0);
    if(doc == NULL) {
        fprintf(stderr, "the document did not parse\n");
        return 1;
    }
    int ok = check_xpath(doc) && check_encoding(doc) && check_gzip("libxml2_check.xml.gz");
    xmlFreeDoc(doc);
    xmlCleanupParser();
    if(!ok)
        return 1;
    printf("libxml2 %s with zlib and iconv: ok\n", LIBXML_DOTTED_VERSION);
    return 0;
}
