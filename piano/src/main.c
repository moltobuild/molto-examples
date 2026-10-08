/* The window: a keyboard drawn with SDL's renderer, played with the mouse (or
   the computer keyboard), heard through SDL's audio and, with R, recorded to a
   file by FFmpeg. */
#include <piano/audio_file.h>
#include <piano/keyboard.h>
#include <piano/synth.h>

#include <SDL3/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SAMPLE_RATE 48000
#define HEADER 44.0f
#define CHUNK 512

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_AudioStream *output;
    SDL_AudioStream *tap; /* what is played, on its way to the recorder */
    piano_synth *synth;
    piano_keyboard keyboard;
    int held[PIANO_KEY_COUNT]; /* mouse and keys holding each piano key */
    int mouse_key;             /* -1 when the mouse holds none */
    bool computer_held[128];
    bool recording;            /* read by the audio thread under the stream lock */
    piano_recorder *recorder;
    char record_path[256];
    const char *record_format;
    char status[256];
} app_state;

/* SDL calls this on its audio thread, with output's lock held. */
static void SDLCALL feed(void *userdata, SDL_AudioStream *stream, int additional, int total) {
    (void)total;
    app_state *app = userdata;
    float buffer[CHUNK];
    int frames = additional / (int)sizeof(float);
    while (frames > 0) {
        const int n = frames < CHUNK ? frames : CHUNK;
        piano_synth_render(app->synth, buffer, n);
        SDL_PutAudioStreamData(stream, buffer, n * (int)sizeof(float));
        if (app->recording)
            SDL_PutAudioStreamData(app->tap, buffer, n * (int)sizeof(float));
        frames -= n;
    }
}

static void press(app_state *app, int key, float velocity) {
    if (key < 0)
        return;
    if (app->held[key]++ == 0) {
        SDL_LockAudioStream(app->output);
        piano_synth_note_on(app->synth, app->keyboard.keys[key].note, velocity);
        SDL_UnlockAudioStream(app->output);
    }
}

static void release(app_state *app, int key) {
    if (key < 0 || app->held[key] == 0)
        return;
    if (--app->held[key] == 0) {
        SDL_LockAudioStream(app->output);
        piano_synth_note_off(app->synth, app->keyboard.keys[key].note);
        SDL_UnlockAudioStream(app->output);
    }
}

/* Lower on the key is louder, as a finger pressing nearer its front edge. */
static float velocity_at(const app_state *app, int key, float y) {
    const piano_rect r = app->keyboard.keys[key].rect;
    const float depth = (y - HEADER - r.y) / r.h;
    return 0.45f + 0.55f * SDL_clamp(depth, 0.0f, 1.0f);
}

static void mouse_to(app_state *app, float x, float y) {
    const int key = piano_keyboard_hit(&app->keyboard, x, y - HEADER);
    if (key == app->mouse_key)
        return;
    /* Dragging across keys plays each one in turn: a glissando. */
    release(app, app->mouse_key);
    app->mouse_key = key;
    if (key >= 0)
        press(app, key, velocity_at(app, key, y));
}

static void drain_tap(app_state *app) {
    float buffer[4096];
    int got;
    while ((got = SDL_GetAudioStreamData(app->tap, buffer, sizeof buffer)) > 0)
        if (app->recorder &&
            !piano_recorder_write(app->recorder, buffer, got / (int)sizeof(float)))
            SDL_Log("could not write to %s", app->record_path);
}

static void toggle_recording(app_state *app) {
    if (!app->recorder) {
        char stamp[32];
        const time_t now = time(NULL);
        strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", localtime(&now));
        snprintf(app->record_path, sizeof app->record_path, "piano-%s.%s", stamp,
                 app->record_format);
        char err[256];
        app->recorder = piano_recorder_open(app->record_path, SAMPLE_RATE, err, sizeof err);
        if (!app->recorder) {
            snprintf(app->status, sizeof app->status, "Cannot record to %s: %s",
                     app->record_path, err);
            return;
        }
        SDL_ClearAudioStream(app->tap);
        SDL_LockAudioStream(app->output);
        app->recording = true;
        SDL_UnlockAudioStream(app->output);
        app->status[0] = '\0';
    } else {
        SDL_LockAudioStream(app->output);
        app->recording = false;
        SDL_UnlockAudioStream(app->output);
        drain_tap(app);
        const double seconds = piano_recorder_seconds(app->recorder);
        const bool ok = piano_recorder_close(app->recorder);
        app->recorder = NULL;
        snprintf(app->status, sizeof app->status, ok ? "Saved %s (%.1f s)" : "Could not finish %s",
                 app->record_path, seconds);
    }
}

static void fill(SDL_Renderer *r, piano_rect rect, Uint8 red, Uint8 green, Uint8 blue) {
    SDL_SetRenderDrawColor(r, red, green, blue, 255);
    SDL_FRect f = {rect.x, rect.y + HEADER, rect.w, rect.h};
    SDL_RenderFillRect(r, &f);
}

static void outline(SDL_Renderer *r, piano_rect rect) {
    SDL_SetRenderDrawColor(r, 40, 40, 46, 255);
    SDL_FRect f = {rect.x, rect.y + HEADER, rect.w, rect.h};
    SDL_RenderRect(r, &f);
}

/* The computer key that plays a piano key, as piano_key_for_char reads them. */
static bool computer_label(int key, char label[2]) {
    static const char keys[] = "AWSEDFTGYHUJKOLP;";
    if (key < 0 || key >= (int)sizeof keys - 1)
        return false;
    label[0] = keys[key];
    label[1] = '\0';
    return true;
}

static void draw(app_state *app) {
    SDL_Renderer *r = app->renderer;
    SDL_SetRenderDrawColor(r, 28, 28, 34, 255);
    SDL_RenderClear(r);

    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < PIANO_KEY_COUNT; i++) {
            const piano_key *key = &app->keyboard.keys[i];
            if (key->black != (pass == 1))
                continue;
            const bool down = app->held[i] > 0;
            if (key->black)
                down ? fill(r, key->rect, 70, 110, 200) : fill(r, key->rect, 24, 24, 28);
            else
                down ? fill(r, key->rect, 160, 195, 255) : fill(r, key->rect, 250, 248, 240);
            outline(r, key->rect);

            const float bottom = key->rect.y + HEADER + key->rect.h;
            const float centre = key->rect.x + key->rect.w / 2;
            if (key->black)
                SDL_SetRenderDrawColor(r, 200, 200, 210, 255);
            else
                SDL_SetRenderDrawColor(r, 90, 90, 100, 255);
            char label[2];
            if (computer_label(i, label))
                SDL_RenderDebugText(r, centre - 4, bottom - 34, label);
            if (key->note % 12 == 0) {
                char name[8];
                piano_note_name(key->note, name, sizeof name);
                SDL_RenderDebugText(r, centre - 4.0f * (float)strlen(name), bottom - 16, name);
            }
        }
    }

    SDL_SetRenderDrawColor(r, 220, 220, 230, 255);
    SDL_RenderDebugText(r, 12, 10, "Click or drag the keys, or type A W S E D F T G Y H U J K O L P ;");
    if (app->recorder) {
        char line[320];
        snprintf(line, sizeof line, "REC %.1f s  ->  %s   (R to stop)",
                 piano_recorder_seconds(app->recorder), app->record_path);
        SDL_SetRenderDrawColor(r, 255, 90, 90, 255);
        SDL_RenderDebugText(r, 12, 26, line);
    } else {
        SDL_RenderDebugText(r, 12, 26,
                            app->status[0] ? app->status : "R: record to a file   Esc: quit");
    }
    SDL_RenderPresent(r);
}

static void relayout(app_state *app) {
    int w, h;
    SDL_GetWindowSize(app->window, &w, &h);
    piano_keyboard_layout(&app->keyboard, (float)w, (float)h - HEADER);
}

static void usage(const char *argv0) {
    fprintf(stderr,
            "usage: %s [--sample FILE [--sample-note N]] [--format EXT]\n"
            "  --sample FILE     play every key from this recording (any format FFmpeg reads)\n"
            "  --sample-note N   the MIDI note the recording sounds at (default 60, middle C)\n"
            "  --format EXT      what R records to: flac (default), wav, m4a, aiff, ...\n",
            argv0);
}

int main(int argc, char **argv) {
    const char *sample_path = NULL;
    int sample_note = 60;
    app_state app = {.mouse_key = -1, .record_format = "flac"};

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--sample") == 0 && i + 1 < argc) {
            sample_path = argv[++i];
        } else if (strcmp(argv[i], "--sample-note") == 0 && i + 1 < argc) {
            sample_note = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--format") == 0 && i + 1 < argc) {
            app.record_format = argv[++i];
        } else {
            usage(argv[0]);
            return strcmp(argv[i], "--help") == 0 ? 0 : 2;
        }
    }

    app.synth = piano_synth_new(SAMPLE_RATE);
    if (!app.synth)
        return 1;
    if (sample_path) {
        piano_audio audio;
        char err[256];
        if (!piano_audio_load(sample_path, SAMPLE_RATE, &audio, err, sizeof err)) {
            fprintf(stderr, "piano: %s\n", err);
            return 1;
        }
        piano_synth_set_sample(app.synth, audio.samples, audio.count, sample_note);
        printf("Playing %s (%.2f s) at MIDI note %d\n", sample_path,
               (double)audio.count / SAMPLE_RATE, sample_note);
        piano_audio_free(&audio);
    }

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        fprintf(stderr, "piano: %s\n", SDL_GetError());
        return 1;
    }
    if (!SDL_CreateWindowAndRenderer("molto piano", 1000, 340, SDL_WINDOW_RESIZABLE,
                                     &app.window, &app.renderer)) {
        fprintf(stderr, "piano: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetWindowMinimumSize(app.window, 600, 220);
    SDL_SetRenderVSync(app.renderer, 1);

    const SDL_AudioSpec spec = {SDL_AUDIO_F32, 1, SAMPLE_RATE};
    app.output = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, feed, &app);
    app.tap = SDL_CreateAudioStream(&spec, &spec);
    if (!app.output || !app.tap) {
        fprintf(stderr, "piano: no audio: %s\n", SDL_GetError());
        return 1;
    }
    SDL_ResumeAudioStreamDevice(app.output);
    relayout(&app);

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                relayout(&app);
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (e.button.button == SDL_BUTTON_LEFT)
                    mouse_to(&app, e.button.x, e.button.y);
                break;
            case SDL_EVENT_MOUSE_MOTION:
                if (e.motion.state & SDL_BUTTON_LMASK)
                    mouse_to(&app, e.motion.x, e.motion.y);
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (e.button.button == SDL_BUTTON_LEFT) {
                    release(&app, app.mouse_key);
                    app.mouse_key = -1;
                }
                break;
            case SDL_EVENT_WINDOW_MOUSE_LEAVE:
                release(&app, app.mouse_key);
                app.mouse_key = -1;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (e.key.key == SDLK_ESCAPE) {
                    running = false;
                } else if (e.key.key == SDLK_R && !e.key.repeat) {
                    toggle_recording(&app);
                } else if (e.key.key < 128 && !e.key.repeat) {
                    const int key = piano_key_for_char((char)e.key.key);
                    if (key >= 0 && !app.computer_held[e.key.key]) {
                        app.computer_held[e.key.key] = true;
                        press(&app, key, 0.8f);
                    }
                }
                break;
            case SDL_EVENT_KEY_UP:
                if (e.key.key < 128 && app.computer_held[e.key.key]) {
                    app.computer_held[e.key.key] = false;
                    release(&app, piano_key_for_char((char)e.key.key));
                }
                break;
            default:
                break;
            }
        }
        if (app.recorder)
            drain_tap(&app);
        draw(&app);
    }

    if (app.recorder)
        toggle_recording(&app);
    SDL_DestroyAudioStream(app.output);
    SDL_DestroyAudioStream(app.tap);
    SDL_DestroyRenderer(app.renderer);
    SDL_DestroyWindow(app.window);
    SDL_Quit();
    piano_synth_free(app.synth);
    if (app.status[0])
        printf("%s\n", app.status);
    return 0;
}
