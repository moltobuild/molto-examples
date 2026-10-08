#ifndef PIANO_AUDIO_FILE_H
#define PIANO_AUDIO_FILE_H

#include <stdbool.h>
#include <stddef.h>

/* Audio files through FFmpeg: any format it reads comes in as mono float,
   and what is played goes out in the format a file's extension names. */

typedef struct {
    float *samples; /* mono, in [-1, 1] */
    size_t count;
} piano_audio;

/* The longest file piano_audio_load keeps, in seconds. */
#define PIANO_AUDIO_MAX_SECONDS 30

/* Decodes the first audio stream of path, down-mixed to mono and resampled to
   sample_rate. On failure returns false with a reason in err. */
bool piano_audio_load(const char *path, int sample_rate, piano_audio *out, char *err,
                      size_t err_size);
void piano_audio_free(piano_audio *audio);

/* Writes mono float samples at sample_rate to path, encoded with the codec its
   extension implies: .flac, .wav, .m4a (AAC), .aiff, … */
typedef struct piano_recorder piano_recorder;

piano_recorder *piano_recorder_open(const char *path, int sample_rate, char *err,
                                    size_t err_size);
bool piano_recorder_write(piano_recorder *rec, const float *samples, int count);
/* Seconds written so far. */
double piano_recorder_seconds(const piano_recorder *rec);
/* Flushes the encoder, finishes the file and frees rec. False if the file could
   not be completed. */
bool piano_recorder_close(piano_recorder *rec);

#endif
