#ifndef WORDCOUNT_COUNT_H
#define WORDCOUNT_COUNT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/* What `wc` reports for one input. */
typedef struct {
    size_t lines;
    size_t words;
    size_t bytes;
} wc_counts;

/* Counts fed in pieces. A word can span two pieces, so whether the last byte
   seen was inside one is part of the state, not something to recompute. */
typedef struct {
    wc_counts counts;
    bool in_word;
} wc_counter;

/* A counter with nothing counted yet. */
[[nodiscard]] wc_counter wc_counter_new(void);

/* Count `length` more bytes of the same input. */
void wc_feed(wc_counter *counter, const char *data, size_t length);

/* Count everything left in `stream`. False when reading it failed, in which
   case `out` holds what was counted before the error. */
[[nodiscard]] bool wc_count_stream(FILE *stream, wc_counts *out);

/* Add `part` into `total`, for the line `wc` prints after several files. */
void wc_add(wc_counts *total, wc_counts part);

#endif
