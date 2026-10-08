#include <piano/keyboard.h>
#include <piano/synth.h>

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define TWO_PI 6.283185307179586
#define HARMONICS 8
/* Below this a voice is inaudible and is freed. */
#define SILENCE 0.0005f

typedef struct {
    bool active;
    bool held;
    int note;
    float velocity;
    double phase;       /* synthesized: cycles into the fundamental, [0, 1) */
    double position;    /* sampled: frames into the sample */
    double step;        /* per output frame: cycles, or sample frames */
    double age;         /* seconds since struck */
    float harmonic[HARMONICS]; /* synthesized: each harmonic's amplitude */
    float level;        /* envelope */
    float decay;        /* per-frame factor while held */
    float release;      /* per-frame factor once let go */
    unsigned long long started; /* to steal the oldest voice */
} voice;

struct piano_synth {
    int sample_rate;
    voice voices[PIANO_SYNTH_VOICES];
    float harmonic_decay[HARMONICS]; /* per-frame factor for each harmonic */
    unsigned long long clock;
    float *sample;
    size_t sample_count;
    int base_note;
};

/* The factor that brings a level down to 1/e in seconds at the sample rate. */
static float per_frame(double seconds, int sample_rate) {
    return (float)exp(-1.0 / (seconds * sample_rate));
}

piano_synth *piano_synth_new(int sample_rate) {
    if (sample_rate <= 0)
        return NULL;
    piano_synth *synth = calloc(1, sizeof *synth);
    if (!synth)
        return NULL;
    synth->sample_rate = sample_rate;
    /* The higher a harmonic, the sooner it dies out. */
    for (int h = 0; h < HARMONICS; h++)
        synth->harmonic_decay[h] = h == 0 ? 1.0f : per_frame(0.55 / h, sample_rate);
    return synth;
}

void piano_synth_free(piano_synth *synth) {
    if (!synth)
        return;
    free(synth->sample);
    free(synth);
}

void piano_synth_set_sample(piano_synth *synth, const float *samples, size_t count,
                            int base_note) {
    free(synth->sample);
    synth->sample = NULL;
    synth->sample_count = 0;
    if (count > 0) {
        synth->sample = malloc(count * sizeof *synth->sample);
        if (!synth->sample)
            return;
        memcpy(synth->sample, samples, count * sizeof *samples);
        synth->sample_count = count;
        synth->base_note = base_note;
    }
    piano_synth_all_notes_off(synth);
    for (int i = 0; i < PIANO_SYNTH_VOICES; i++)
        synth->voices[i].active = false;
}

static voice *free_voice(piano_synth *synth, int note) {
    voice *oldest = NULL;
    for (int i = 0; i < PIANO_SYNTH_VOICES; i++) {
        voice *v = &synth->voices[i];
        if (v->active && v->note == note)
            return v; /* the same string struck again */
    }
    for (int i = 0; i < PIANO_SYNTH_VOICES; i++) {
        voice *v = &synth->voices[i];
        if (!v->active)
            return v;
        if (!oldest || v->started < oldest->started)
            oldest = v;
    }
    return oldest;
}

void piano_synth_note_on(piano_synth *synth, int note, float velocity) {
    if (velocity <= 0)
        return;
    if (velocity > 1)
        velocity = 1;
    voice *v = free_voice(synth, note);
    const double frequency = piano_note_frequency(note);
    *v = (voice){
        .active = true,
        .held = true,
        .note = note,
        .velocity = velocity,
        .level = 1,
        /* Lower strings ring longer, as on a piano. */
        .decay = per_frame(2.5 * pow(2.0, (60 - note) / 24.0), synth->sample_rate),
        .release = per_frame(0.12, synth->sample_rate),
        .started = ++synth->clock,
    };
    if (synth->sample_count > 0) {
        v->step = frequency / piano_note_frequency(synth->base_note);
    } else {
        v->step = frequency / synth->sample_rate;
        /* Harmonics above half the sample rate would alias; they stay silent. */
        for (int h = 0; h < HARMONICS; h++)
            v->harmonic[h] = frequency * (h + 1) < synth->sample_rate / 2.0
                                 ? (float)pow(h + 1, -1.6)
                                 : 0.0f;
    }
}

void piano_synth_note_off(piano_synth *synth, int note) {
    for (int i = 0; i < PIANO_SYNTH_VOICES; i++)
        if (synth->voices[i].active && synth->voices[i].note == note)
            synth->voices[i].held = false;
}

void piano_synth_all_notes_off(piano_synth *synth) {
    for (int i = 0; i < PIANO_SYNTH_VOICES; i++)
        synth->voices[i].held = false;
}

/* A struck string: the harmonics fall off with their number and the higher
   ones die out first, so the tone darkens as it fades. */
static float synthesized(const piano_synth *synth, voice *v) {
    float sum = 0;
    for (int h = 0; h < HARMONICS; h++) {
        if (v->harmonic[h] == 0)
            continue;
        sum += v->harmonic[h] * (float)sin(TWO_PI * v->phase * (h + 1));
        v->harmonic[h] *= synth->harmonic_decay[h];
    }
    return sum;
}

static float sampled(const piano_synth *synth, voice *v) {
    const size_t i = (size_t)v->position;
    if (i + 1 >= synth->sample_count) {
        v->level = 0; /* the sample has ended */
        return 0;
    }
    const float t = (float)(v->position - (double)i);
    return synth->sample[i] * (1 - t) + synth->sample[i + 1] * t;
}

void piano_synth_render(piano_synth *synth, float *out, int frames) {
    const double dt = 1.0 / synth->sample_rate;
    /* A short ramp at the start of each note avoids a click. */
    const double attack = 0.004;
    for (int f = 0; f < frames; f++) {
        float mix = 0;
        for (int i = 0; i < PIANO_SYNTH_VOICES; i++) {
            voice *v = &synth->voices[i];
            if (!v->active)
                continue;
            float s;
            if (synth->sample_count > 0) {
                s = sampled(synth, v);
                v->position += v->step;
            } else {
                s = synthesized(synth, v);
                v->phase += v->step;
                v->phase -= floor(v->phase);
            }
            const float ramp = v->age < attack ? (float)(v->age / attack) : 1.0f;
            mix += s * v->level * v->velocity * ramp;
            v->age += dt;
            v->level *= v->held ? v->decay : v->release;
            if (v->level < SILENCE)
                v->active = false;
        }
        /* Several voices together stay inside [-1, 1] without clipping hard. */
        out[f] = tanhf(mix * 0.35f);
    }
}

int piano_synth_active_voices(const piano_synth *synth) {
    int n = 0;
    for (int i = 0; i < PIANO_SYNTH_VOICES; i++)
        n += synth->voices[i].active;
    return n;
}
