#include <moltest.h>

#include <piano/keyboard.h>
#include <piano/synth.h>

#include <math.h>

#define RATE 48000

static float peak(const float *s, int n) {
    float p = 0;
    for (int i = 0; i < n; i++)
        p = fmaxf(p, fabsf(s[i]));
    return p;
}

/* Upward zero crossings per second: the fundamental of a clean tone. */
static double pitch(const float *s, int n) {
    int crossings = 0;
    for (int i = 1; i < n; i++)
        crossings += s[i - 1] < 0 && s[i] >= 0;
    return crossings * (double)RATE / n;
}

static float buffer[RATE];

DESCRIBE(nothing_played_is_silence) {
    piano_synth *synth = piano_synth_new(RATE);
    ASSERT_NOT_NULL(synth);
    piano_synth_render(synth, buffer, 4800);
    EXPECT_TRUE(peak(buffer, 4800) == 0.0f);
    EXPECT_EQ(0, piano_synth_active_voices(synth));
    piano_synth_free(synth);
}

DESCRIBE(a_note_sounds_inside_the_range_and_at_its_pitch) {
    piano_synth *synth = piano_synth_new(RATE);
    piano_synth_note_on(synth, 69, 1.0f); /* A4 */
    piano_synth_render(synth, buffer, RATE / 2);
    EXPECT_TRUE(peak(buffer, RATE / 2) > 0.1f);
    EXPECT_TRUE(peak(buffer, RATE / 2) <= 1.0f);
    EXPECT_TRUE(fabs(pitch(buffer, RATE / 2) - 440.0) < 10.0);
    EXPECT_EQ(1, piano_synth_active_voices(synth));
    piano_synth_free(synth);
}

DESCRIBE(a_released_note_fades_out_and_frees_its_voice) {
    piano_synth *synth = piano_synth_new(RATE);
    piano_synth_note_on(synth, 60, 1.0f);
    piano_synth_render(synth, buffer, 2400);
    piano_synth_note_off(synth, 60);
    piano_synth_render(synth, buffer, RATE);
    EXPECT_EQ(0, piano_synth_active_voices(synth));
    EXPECT_TRUE(peak(buffer + RATE - 4800, 4800) < 0.001f);
    piano_synth_free(synth);
}

DESCRIBE(chords_play_one_voice_per_note_and_never_clip) {
    piano_synth *synth = piano_synth_new(RATE);
    const int chord[] = {60, 64, 67, 72, 76, 79, 84};
    for (int i = 0; i < 7; i++)
        piano_synth_note_on(synth, chord[i], 1.0f);
    EXPECT_EQ(7, piano_synth_active_voices(synth));
    piano_synth_note_on(synth, 60, 1.0f); /* struck again: the same voice */
    EXPECT_EQ(7, piano_synth_active_voices(synth));
    piano_synth_render(synth, buffer, RATE / 4);
    EXPECT_TRUE(peak(buffer, RATE / 4) <= 1.0f);
    piano_synth_free(synth);
}

DESCRIBE(more_notes_than_voices_steal_the_oldest) {
    piano_synth *synth = piano_synth_new(RATE);
    for (int i = 0; i < PIANO_SYNTH_VOICES + 5; i++)
        piano_synth_note_on(synth, 30 + i, 0.5f);
    EXPECT_EQ(PIANO_SYNTH_VOICES, piano_synth_active_voices(synth));
    piano_synth_free(synth);
}

DESCRIBE(a_sample_is_played_faster_for_higher_notes) {
    /* One second of a 220 Hz sine, sounding at A3 (MIDI 57). */
    static float sine[RATE];
    for (int i = 0; i < RATE; i++)
        sine[i] = (float)sin(6.283185307179586 * 220.0 * i / RATE);
    piano_synth *synth = piano_synth_new(RATE);
    piano_synth_set_sample(synth, sine, RATE, 57);

    piano_synth_note_on(synth, 69, 1.0f); /* an octave up: 440 Hz */
    piano_synth_render(synth, buffer, RATE / 4);
    EXPECT_TRUE(fabs(pitch(buffer, RATE / 4) - 440.0) < 10.0);

    /* Played twice as fast, the one-second sample is over in half a second. */
    piano_synth_render(synth, buffer, RATE / 2);
    EXPECT_EQ(0, piano_synth_active_voices(synth));
    piano_synth_free(synth);
}
