# piano

A piano you play with the mouse: click a key to hear it, drag across the keys
for a glissando. SDL 3 draws the window and plays the sound; FFmpeg reads the
recordings a key can play and writes down what you play.

```sh
molto run                                   # open the keyboard
molto run -- --sample my-note.wav           # every key plays that recording
molto run -- --format m4a                   # R records to AAC instead of FLAC
molto test                                  # layout, synth and FFmpeg round trips
```

Both libraries come from the molto registry as source recipes, compiled into
the build, so nothing is installed system-wide:

```toml
[deps]
sdl3 = "3.4.18"    # configured by SDL's own CMake, every subsystem it builds here
ffmpeg = "9.0.2"   # configured by FFmpeg's own configure, with zlib, bzip2, xz and libiconv
```

## What it does

- **Two octaves and a top C** (C4 to C6). Click lower on a key to play it
  louder; drag to slide from key to key.
- **The computer keyboard plays too**: `A S D F G H J K L ;` are the white keys
  from C4 to E5 and `W E T Y U O P` the black keys between them, as printed on
  the keys.
- **Polyphony**: up to 32 notes at once. A note rings and fades while held, and
  dies away quickly once let go.
- **Synthesized by default**: eight harmonics per note, the higher ones dying
  first, so the tone darkens as it fades.
- **`--sample FILE`** plays any file FFmpeg reads (WAV, FLAC, MP3, Ogg, an MP4's
  soundtrack, …), decoded and resampled to mono 48 kHz with libswresample, then
  sped up or slowed down for each key. `--sample-note N` says which MIDI note
  the recording sounds at (60, middle C, by default).
- **R records** what you play to `piano-YYYYMMDD-HHMMSS.flac` in the current
  directory, and R again stops. `--format` picks the container and codec by
  extension: `flac`, `wav`, `m4a` (AAC), `aiff`, …

## Layout

| Path | What |
|---|---|
| `src/keyboard.c`, `include/piano/keyboard.h` | Where each key is, which one a point hits, note names and frequencies |
| `src/synth.c`, `include/piano/synth.h` | The voices: synthesized or sampled, with their envelopes, mixed to mono float |
| `src/audio_file.c`, `include/piano/audio_file.h` | FFmpeg: decode and resample any file in; encode and mux a recording out |
| `src/main.c` | The SDL window, its input, and the audio stream the synth fills |
| `tests/` | moltest suites: hit testing, pitch and fade of the synth, FLAC/WAV/AAC written and read back |

## How the sound moves

SDL calls the synth on its audio thread to fill the output stream, holding that
stream's lock; the window takes the same lock to start and stop notes. While
recording, the audio thread also copies each buffer into a second SDL audio
stream used as a queue, and the window drains it into the FFmpeg encoder, so
encoding never runs on the audio thread.
