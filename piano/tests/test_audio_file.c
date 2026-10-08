#include <moltest.h>

#include <piano/audio_file.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* Recordings written by FFmpeg and read back by FFmpeg, through real files in
   the system's temporary directory. */

#define RATE 48000

static const char *temp_path(const char *name) {
    static char path[512];
    const char *dir = getenv("TMPDIR");
    if (!dir)
        dir = getenv("TEMP");
    if (!dir)
        dir = "/tmp";
    snprintf(path, sizeof path, "%s/%s", dir, name);
    return path;
}

static double pitch(const float *s, size_t n, int rate) {
    int crossings = 0;
    for (size_t i = 1; i < n; i++)
        crossings += s[i - 1] < 0 && s[i] >= 0;
    return crossings * (double)rate / (double)n;
}

/* Records one second of 440 Hz and loads it back at load_rate. */
static void round_trip(const char *name, int load_rate, double tolerance_seconds) {
    static float tone[RATE];
    for (int i = 0; i < RATE; i++)
        tone[i] = 0.5f * (float)sin(6.283185307179586 * 440.0 * i / RATE);

    const char *path = temp_path(name);
    char err[256] = "";
    piano_recorder *rec = piano_recorder_open(path, RATE, err, sizeof err);
    if (!rec)
        fprintf(stderr, "%s: %s\n", name, err);
    ASSERT_NOT_NULL(rec);
    /* In pieces of an odd size, as the audio thread hands them over. */
    for (int at = 0; at < RATE; at += 1000)
        ASSERT_TRUE(piano_recorder_write(rec, tone + at, RATE - at < 1000 ? RATE - at : 1000));
    EXPECT_TRUE(fabs(piano_recorder_seconds(rec) - 1.0) < 1e-9);
    ASSERT_TRUE(piano_recorder_close(rec));

    piano_audio audio;
    if (!piano_audio_load(path, load_rate, &audio, err, sizeof err))
        fprintf(stderr, "%s: %s\n", name, err);
    ASSERT_NOT_NULL(audio.samples);
    const double seconds = (double)audio.count / load_rate;
    EXPECT_TRUE(fabs(seconds - 1.0) < tolerance_seconds);
    EXPECT_TRUE(fabs(pitch(audio.samples, audio.count, load_rate) - 440.0) < 5.0);
    piano_audio_free(&audio);
    remove(path);
}

DESCRIBE(a_flac_recording_reads_back_as_recorded) {
    round_trip("molto-piano-test.flac", RATE, 0.001);
}

DESCRIBE(a_wav_recording_reads_back_resampled) {
    round_trip("molto-piano-test.wav", 44100, 0.001);
}

DESCRIBE(an_aac_recording_reads_back_at_its_pitch) {
    /* AAC frames are fixed-size and add priming: the length is close, not exact. */
    round_trip("molto-piano-test.m4a", RATE, 0.1);
}

DESCRIBE(a_missing_file_is_an_error_with_a_reason) {
    piano_audio audio;
    char err[256] = "";
    EXPECT_FALSE(piano_audio_load(temp_path("molto-piano-missing.wav"), RATE, &audio, err,
                                  sizeof err));
    EXPECT_NULL(audio.samples);
    EXPECT_TRUE(err[0] != '\0');
}

DESCRIBE(a_file_name_without_an_audio_format_is_refused) {
    char err[256] = "";
    EXPECT_NULL(piano_recorder_open(temp_path("molto-piano-test.nothing"), RATE, err,
                                    sizeof err));
    EXPECT_TRUE(err[0] != '\0');
}
