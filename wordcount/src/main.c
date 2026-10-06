#include <wordcount/count.h>

#include <stdio.h>
#include <string.h>

/* Molto passes both from Project.toml; the fallbacks are for any other build. */
#ifndef MOLTO_PKG_NAME
#define MOLTO_PKG_NAME "wordcount"
#endif
#ifndef MOLTO_PKG_VERSION
#define MOLTO_PKG_VERSION "0.0.0-unknown"
#endif

static void print_counts(wc_counts counts, const char *name) {
    printf("%7zu %7zu %7zu", counts.lines, counts.words, counts.bytes);
    if(name != NULL)
        printf(" %s", name);
    printf("\n");
}

int main(int argc, char **argv) {
    if(argc == 2 && strcmp(argv[1], "--version") == 0) {
        printf("%s %s\n", MOLTO_PKG_NAME, MOLTO_PKG_VERSION);
        return 0;
    }

    /* No files: count standard input, like wc. */
    if(argc < 2) {
        wc_counts counts;
        if(!wc_count_stream(stdin, &counts)) {
            fprintf(stderr, "wordcount: could not read standard input\n");
            return 1;
        }
        print_counts(counts, NULL);
        return 0;
    }

    /* A file that cannot be read is reported and skipped, and the exit status
       says so; the others are still counted. */
    int status = 0;
    wc_counts total = {0, 0, 0};
    for(int i = 1; i < argc; i++) {
        FILE *file = fopen(argv[i], "rb");
        if(file == NULL) {
            fprintf(stderr, "wordcount: %s: cannot open\n", argv[i]);
            status = 1;
            continue;
        }
        wc_counts counts;
        const bool ok = wc_count_stream(file, &counts);
        fclose(file);
        if(!ok) {
            fprintf(stderr, "wordcount: %s: read error\n", argv[i]);
            status = 1;
            continue;
        }
        print_counts(counts, argv[i]);
        wc_add(&total, counts);
    }
    if(argc > 2)
        print_counts(total, "total");
    return status;
}
