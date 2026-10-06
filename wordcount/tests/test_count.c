/* Linked against src/ minus main.c (molto's per_file mode), so this file
   brings its own main() and reports through its exit status. */
#include <wordcount/count.h>

#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(expr)                                                        \
    do {                                                                   \
        if (!(expr)) {                                                     \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #expr); \
            failures++;                                                    \
        }                                                                  \
    } while (0)

static wc_counts count_text(const char *text) {
    wc_counter counter = wc_counter_new();
    wc_feed(&counter, text, strlen(text));
    return counter.counts;
}

static void empty_input_counts_nothing(void) {
    const wc_counts c = count_text("");
    CHECK(c.lines == 0 && c.words == 0 && c.bytes == 0);
}

static void a_last_line_without_newline_has_words_but_no_line(void) {
    const wc_counts c = count_text("hello world");
    CHECK(c.lines == 0);
    CHECK(c.words == 2);
    CHECK(c.bytes == 11);
}

static void runs_of_whitespace_separate_words_once(void) {
    const wc_counts c = count_text("  one\t\ttwo \n\nthree  \n");
    CHECK(c.lines == 3);
    CHECK(c.words == 3);
    CHECK(c.bytes == 21);
}

/* The reason wc_counter carries in_word: a word split across two reads is
   still one word. */
static void a_word_split_across_two_feeds_counts_once(void) {
    wc_counter counter = wc_counter_new();
    wc_feed(&counter, "hel", 3);
    wc_feed(&counter, "lo there\n", 9);
    CHECK(counter.counts.words == 2);
    CHECK(counter.counts.lines == 1);
    CHECK(counter.counts.bytes == 12);
}

static void a_stream_is_counted_to_its_end(void) {
    FILE *file = tmpfile();
    CHECK(file != NULL);
    if (file == NULL)
        return;
    fputs("first line\nsecond line here\n", file);
    rewind(file);

    wc_counts c;
    CHECK(wc_count_stream(file, &c));
    CHECK(c.lines == 2);
    CHECK(c.words == 5);
    CHECK(c.bytes == 28);
    fclose(file);
}

static void totals_add_every_field(void) {
    wc_counts total = {1, 2, 3};
    wc_add(&total, (wc_counts){10, 20, 30});
    CHECK(total.lines == 11 && total.words == 22 && total.bytes == 33);
}

int main(void) {
    empty_input_counts_nothing();
    a_last_line_without_newline_has_words_but_no_line();
    runs_of_whitespace_separate_words_once();
    a_word_split_across_two_feeds_counts_once();
    a_stream_is_counted_to_its_end();
    totals_add_every_field();

    if (failures > 0) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    printf("all checks passed\n");
    return 0;
}
