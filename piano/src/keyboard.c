#include <piano/keyboard.h>

#include <math.h>
#include <stdio.h>

static bool is_black(int note) {
    switch (note % 12) {
    case 1: case 3: case 6: case 8: case 10: return true;
    default: return false;
    }
}

void piano_keyboard_layout(piano_keyboard *kb, float width, float height) {
    int whites = 0;
    for (int i = 0; i < PIANO_KEY_COUNT; i++)
        whites += !is_black(PIANO_FIRST_NOTE + i);

    const float white_w = width / (float)whites;
    const float black_w = white_w * 0.6f;
    const float black_h = height * 0.62f;

    kb->width = width;
    kb->height = height;
    int white_index = 0;
    for (int i = 0; i < PIANO_KEY_COUNT; i++) {
        piano_key *key = &kb->keys[i];
        key->note = PIANO_FIRST_NOTE + i;
        key->black = is_black(key->note);
        if (key->black) {
            /* On the boundary between the white key before it and the next. */
            const float boundary = (float)white_index * white_w;
            key->rect = (piano_rect){boundary - black_w / 2, 0, black_w, black_h};
        } else {
            key->rect = (piano_rect){(float)white_index * white_w, 0, white_w, height};
            white_index++;
        }
    }
}

static bool inside(piano_rect r, float x, float y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

int piano_keyboard_hit(const piano_keyboard *kb, float x, float y) {
    for (int i = 0; i < PIANO_KEY_COUNT; i++)
        if (kb->keys[i].black && inside(kb->keys[i].rect, x, y))
            return i;
    for (int i = 0; i < PIANO_KEY_COUNT; i++)
        if (!kb->keys[i].black && inside(kb->keys[i].rect, x, y))
            return i;
    return -1;
}

double piano_note_frequency(int note) {
    return 440.0 * pow(2.0, (note - 69) / 12.0);
}

void piano_note_name(int note, char *out, int size) {
    static const char *const names[12] = {"C",  "C#", "D",  "D#", "E",  "F",
                                          "F#", "G",  "G#", "A",  "A#", "B"};
    snprintf(out, (size_t)size, "%s%d", names[note % 12], note / 12 - 1);
}

int piano_key_for_char(char c) {
    /* Semitone by semitone from C4: the home row holds the white keys and the
       row above it the black ones, as on a piano seen from the front. */
    static const char row[] = "awsedftgyhujkolp;";
    if (c >= 'A' && c <= 'Z')
        c = (char)(c - 'A' + 'a');
    for (int i = 0; row[i]; i++)
        if (row[i] == c)
            return i;
    return -1;
}
