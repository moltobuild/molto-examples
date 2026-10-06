#include <wordcount/count.h>

#include <ctype.h>

wc_counter wc_counter_new(void) { return (wc_counter){.counts = {0, 0, 0}, .in_word = false}; }

void wc_feed(wc_counter *counter, const char *data, size_t length) {
    for(size_t i = 0; i < length; i++) {
        const unsigned char c = (unsigned char)data[i];
        counter->counts.bytes++;
        if(c == '\n')
            counter->counts.lines++;
        if(isspace(c)) {
            counter->in_word = false;
        } else if(!counter->in_word) {
            counter->in_word = true;
            counter->counts.words++;
        }
    }
}

bool wc_count_stream(FILE *stream, wc_counts *out) {
    wc_counter counter = wc_counter_new();
    char buffer[4096];
    size_t got;
    while((got = fread(buffer, 1, sizeof buffer, stream)) > 0)
        wc_feed(&counter, buffer, got);
    *out = counter.counts;
    return !ferror(stream);
}

void wc_add(wc_counts *total, wc_counts part) {
    total->lines += part.lines;
    total->words += part.words;
    total->bytes += part.bytes;
}
