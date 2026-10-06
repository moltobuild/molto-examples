/* Converts a fixed string through several encodings and back, and fails
   unless every byte comes out as expected on this platform. */

#include <iconv.h>
#include <localcharset.h>
#include <stdio.h>
#include <string.h>

static int convert(const char *to, const char *from, const char *in, size_t in_len, char *out,
                   size_t out_size, size_t *out_len, int report) {
    iconv_t cd = iconv_open(to, from);
    if(cd == (iconv_t)-1) {
        fprintf(stderr, "iconv_open(%s, %s) failed\n", to, from);
        return 1;
    }
    char *inp = (char *)in;
    char *outp = out;
    size_t in_left = in_len;
    size_t out_left = out_size;
    size_t r = iconv(cd, &inp, &in_left, &outp, &out_left);
    iconv_close(cd);
    if(r == (size_t)-1 || in_left != 0) {
        if(report)
            fprintf(stderr, "iconv %s -> %s failed\n", from, to);
        return 1;
    }
    *out_len = out_size - out_left;
    return 0;
}

int main(void) {
    static const char utf8[] = "caf\xc3\xa9 \xe2\x82\xac \xe6\x97\xa5\xe6\x9c\xac";
    static const char latin9_expected[] = "caf\xe9 \xa4";
    char buf[128];
    char back[128];
    size_t n = 0;
    size_t m = 0;

    if(convert("ISO-8859-15", "UTF-8", utf8, 9, buf, sizeof buf, &n, 1) != 0 ||
       n != 6 || memcmp(buf, latin9_expected, 6) != 0) {
        fprintf(stderr, "UTF-8 -> ISO-8859-15 mismatch\n");
        return 1;
    }
    if(convert("SHIFT_JIS", "UTF-8", utf8, sizeof utf8 - 1, buf, sizeof buf, &n, 0) == 0) {
        fprintf(stderr, "the euro sign has no SHIFT_JIS form and must be refused\n");
        return 1;
    }
    if(convert("UTF-16LE", "UTF-8", utf8, sizeof utf8 - 1, buf, sizeof buf, &n, 1) != 0 ||
       convert("UTF-8", "UTF-16LE", buf, n, back, sizeof back, &m, 1) != 0 ||
       m != sizeof utf8 - 1 || memcmp(back, utf8, m) != 0) {
        fprintf(stderr, "UTF-8 <-> UTF-16LE round trip mismatch\n");
        return 1;
    }
    printf("libiconv %d.%d, locale charset %s: ok\n", _libiconv_version >> 8,
           _libiconv_version & 0xff, locale_charset());
    return 0;
}
