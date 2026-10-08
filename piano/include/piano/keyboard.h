#ifndef PIANO_KEYBOARD_H
#define PIANO_KEYBOARD_H

#include <stdbool.h>

/* Two octaves and a top C, from C4 (MIDI 60) to C6 (MIDI 84). */
#define PIANO_FIRST_NOTE 60
#define PIANO_KEY_COUNT 25

typedef struct {
    float x, y, w, h;
} piano_rect;

typedef struct {
    int note;     /* MIDI note number */
    bool black;
    piano_rect rect;
} piano_key;

typedef struct {
    piano_key keys[PIANO_KEY_COUNT];
    float width, height;
} piano_keyboard;

/* Lays the keys out to fill a width x height area: white keys side by side,
   black keys narrower, shorter and centred on the gap between two white keys. */
void piano_keyboard_layout(piano_keyboard *kb, float width, float height);

/* The key under the point, or -1. Black keys sit on top of white ones, so they
   win where the two overlap. */
int piano_keyboard_hit(const piano_keyboard *kb, float x, float y);

/* Equal temperament, A4 (MIDI 69) = 440 Hz. */
double piano_note_frequency(int note);

/* "C4", "F#5": writes at most size bytes, always terminated. */
void piano_note_name(int note, char *out, int size);

/* The key a computer keyboard key plays, by its character on a QWERTY layout:
   A S D F G H J K L ; are the white keys from C4 to E5, and W E T Y U O P the
   black keys between them. -1 if none. */
int piano_key_for_char(char c);

#endif
