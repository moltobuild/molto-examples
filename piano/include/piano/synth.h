#ifndef PIANO_SYNTH_H
#define PIANO_SYNTH_H

#include <stddef.h>

/* A polyphonic voice per note played, mixed into one mono float channel.
   Each voice is either synthesized (a sum of harmonics that fades like a struck
   string) or, once a sample is set, that sample played faster or slower to
   reach the note's pitch. Not thread-safe: the caller serializes note changes
   against rendering. */
typedef struct piano_synth piano_synth;

#define PIANO_SYNTH_VOICES 32

piano_synth *piano_synth_new(int sample_rate);
void piano_synth_free(piano_synth *synth);

/* Plays every note from samples (mono, at the synth's sample rate), which
   sound at base_note when played unchanged. The synth keeps its own copy;
   count 0 goes back to the synthesized tone. */
void piano_synth_set_sample(piano_synth *synth, const float *samples, size_t count,
                            int base_note);

/* velocity in (0, 1]. A note already sounding is struck again. */
void piano_synth_note_on(piano_synth *synth, int note, float velocity);
/* Lets the note ring out over its release instead of stopping it dead. */
void piano_synth_note_off(piano_synth *synth, int note);
void piano_synth_all_notes_off(piano_synth *synth);

/* Writes frames samples in [-1, 1] to out, replacing what was there. */
void piano_synth_render(piano_synth *synth, float *out, int frames);

/* Voices still audible, held or releasing. */
int piano_synth_active_voices(const piano_synth *synth);

#endif
