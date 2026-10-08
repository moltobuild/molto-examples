#include <piano/audio_file.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>

#include <stdio.h>
#include <stdlib.h>

static void set_error(char *err, size_t err_size, const char *what, int code) {
    if (!err || err_size == 0)
        return;
    if (code < 0) {
        char reason[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(code, reason, sizeof reason);
        snprintf(err, err_size, "%s: %s", what, reason);
    } else {
        snprintf(err, err_size, "%s", what);
    }
}

/* ---- Loading ---------------------------------------------------------- */

typedef struct {
    float *samples;
    size_t count, capacity, limit;
} growing;

static bool reserve(growing *g, size_t more) {
    if (g->count + more <= g->capacity)
        return true;
    size_t capacity = g->capacity ? g->capacity : 1 << 16;
    while (capacity < g->count + more)
        capacity *= 2;
    float *samples = realloc(g->samples, capacity * sizeof *samples);
    if (!samples)
        return false;
    g->samples = samples;
    g->capacity = capacity;
    return true;
}

/* Converts in_count frames of in (NULL to drain) and appends them. */
static int append_converted(SwrContext *swr, growing *g, const uint8_t **in, int in_count) {
    const int room = swr_get_out_samples(swr, in_count);
    if (room <= 0)
        return 0;
    if (!reserve(g, (size_t)room))
        return AVERROR(ENOMEM);
    uint8_t *dst = (uint8_t *)(g->samples + g->count);
    const int got = swr_convert(swr, &dst, room, in, in_count);
    if (got < 0)
        return got;
    g->count += (size_t)got;
    if (g->count > g->limit)
        g->count = g->limit;
    return 0;
}

bool piano_audio_load(const char *path, int sample_rate, piano_audio *out, char *err,
                      size_t err_size) {
    *out = (piano_audio){0};
    AVFormatContext *format = NULL;
    AVCodecContext *decoder = NULL;
    SwrContext *swr = NULL;
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    growing g = {.limit = (size_t)sample_rate * PIANO_AUDIO_MAX_SECONDS};
    bool ok = false;
    int rc;

    if (!packet || !frame) {
        set_error(err, err_size, "out of memory", 0);
        goto done;
    }
    if ((rc = avformat_open_input(&format, path, NULL, NULL)) < 0) {
        set_error(err, err_size, path, rc);
        goto done;
    }
    if ((rc = avformat_find_stream_info(format, NULL)) < 0) {
        set_error(err, err_size, "reading the streams", rc);
        goto done;
    }
    const AVCodec *codec = NULL;
    const int stream = av_find_best_stream(format, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
    if (stream < 0) {
        set_error(err, err_size, "no audio stream", stream);
        goto done;
    }
    decoder = avcodec_alloc_context3(codec);
    if (!decoder ||
        (rc = avcodec_parameters_to_context(decoder, format->streams[stream]->codecpar)) < 0 ||
        (rc = avcodec_open2(decoder, codec, NULL)) < 0) {
        set_error(err, err_size, "opening the decoder", decoder ? rc : AVERROR(ENOMEM));
        goto done;
    }

    const AVChannelLayout mono = AV_CHANNEL_LAYOUT_MONO;
    bool draining = false;
    while (!draining && g.count < g.limit) {
        rc = av_read_frame(format, packet);
        if (rc == AVERROR_EOF) {
            draining = true;
            rc = avcodec_send_packet(decoder, NULL);
        } else if (rc < 0) {
            set_error(err, err_size, "reading", rc);
            goto done;
        } else {
            rc = packet->stream_index == stream ? avcodec_send_packet(decoder, packet) : 0;
            av_packet_unref(packet);
        }
        if (rc < 0 && rc != AVERROR(EAGAIN)) {
            set_error(err, err_size, "decoding", rc);
            goto done;
        }
        while ((rc = avcodec_receive_frame(decoder, frame)) >= 0) {
            if (!swr) {
                /* Set up from the first frame: what the decoder really gives. */
                AVChannelLayout layout;
                if (frame->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC)
                    av_channel_layout_default(&layout, frame->ch_layout.nb_channels);
                else
                    av_channel_layout_copy(&layout, &frame->ch_layout);
                rc = swr_alloc_set_opts2(&swr, &mono, AV_SAMPLE_FMT_FLT, sample_rate, &layout,
                                         frame->format, frame->sample_rate, 0, NULL);
                av_channel_layout_uninit(&layout);
                if (rc < 0 || (rc = swr_init(swr)) < 0) {
                    set_error(err, err_size, "setting up the resampler", rc);
                    goto done;
                }
            }
            rc = append_converted(swr, &g, (const uint8_t **)frame->extended_data,
                                  frame->nb_samples);
            av_frame_unref(frame);
            if (rc < 0) {
                set_error(err, err_size, "resampling", rc);
                goto done;
            }
        }
        if (rc != AVERROR(EAGAIN) && rc != AVERROR_EOF) {
            set_error(err, err_size, "decoding", rc);
            goto done;
        }
    }
    if (swr && (rc = append_converted(swr, &g, NULL, 0)) < 0) {
        set_error(err, err_size, "resampling", rc);
        goto done;
    }
    if (g.count == 0) {
        set_error(err, err_size, "the file has no audio", 0);
        goto done;
    }
    out->samples = g.samples;
    out->count = g.count;
    g.samples = NULL;
    ok = true;

done:
    free(g.samples);
    swr_free(&swr);
    avcodec_free_context(&decoder);
    avformat_close_input(&format);
    av_frame_free(&frame);
    av_packet_free(&packet);
    return ok;
}

void piano_audio_free(piano_audio *audio) {
    free(audio->samples);
    *audio = (piano_audio){0};
}

/* ---- Recording -------------------------------------------------------- */

struct piano_recorder {
    AVFormatContext *format;
    AVCodecContext *encoder;
    AVStream *stream;
    SwrContext *swr;
    AVAudioFifo *fifo; /* converted samples waiting for a whole frame */
    AVFrame *frame;
    AVPacket *packet;
    uint8_t **converted;
    int converted_capacity;
    int frame_size;
    int64_t pts;
    int64_t written; /* input frames */
    int sample_rate;
    bool header_written;
};

/* The encoder's preference among what it supports: float, then 16-bit. */
static enum AVSampleFormat pick_format(const AVCodecContext *ctx, const AVCodec *codec) {
    const enum AVSampleFormat *formats = NULL;
    int n = 0;
    if (avcodec_get_supported_config(ctx, codec, AV_CODEC_CONFIG_SAMPLE_FORMAT, 0,
                                     (const void **)&formats, &n) < 0 ||
        !formats || n == 0)
        return AV_SAMPLE_FMT_FLT;
    const enum AVSampleFormat wanted[] = {AV_SAMPLE_FMT_FLT, AV_SAMPLE_FMT_FLTP,
                                          AV_SAMPLE_FMT_S16, AV_SAMPLE_FMT_S16P};
    for (size_t w = 0; w < sizeof wanted / sizeof *wanted; w++)
        for (int i = 0; i < n; i++)
            if (formats[i] == wanted[w])
                return formats[i];
    return formats[0];
}

/* sample_rate if the encoder takes it, otherwise the closest one it does. */
static int pick_rate(const AVCodecContext *ctx, const AVCodec *codec, int sample_rate) {
    const int *rates = NULL;
    int n = 0;
    if (avcodec_get_supported_config(ctx, codec, AV_CODEC_CONFIG_SAMPLE_RATE, 0,
                                     (const void **)&rates, &n) < 0 ||
        !rates || n == 0)
        return sample_rate;
    int best = rates[0];
    for (int i = 0; i < n; i++)
        if (abs(rates[i] - sample_rate) < abs(best - sample_rate))
            best = rates[i];
    return best;
}

static int write_packets(piano_recorder *rec) {
    int rc;
    while ((rc = avcodec_receive_packet(rec->encoder, rec->packet)) >= 0) {
        av_packet_rescale_ts(rec->packet, rec->encoder->time_base, rec->stream->time_base);
        rec->packet->stream_index = rec->stream->index;
        rc = av_interleaved_write_frame(rec->format, rec->packet);
        if (rc < 0)
            return rc;
    }
    return rc == AVERROR(EAGAIN) || rc == AVERROR_EOF ? 0 : rc;
}

/* Encodes count samples from the fifo. */
static int encode(piano_recorder *rec, int count) {
    AVFrame *frame = rec->frame;
    av_frame_unref(frame);
    frame->nb_samples = count;
    frame->format = rec->encoder->sample_fmt;
    frame->sample_rate = rec->encoder->sample_rate;
    int rc = av_channel_layout_copy(&frame->ch_layout, &rec->encoder->ch_layout);
    if (rc < 0 || (rc = av_frame_get_buffer(frame, 0)) < 0)
        return rc;
    if (av_audio_fifo_read(rec->fifo, (void **)frame->data, count) < count)
        return AVERROR_BUG;
    frame->pts = rec->pts;
    rec->pts += count;
    if ((rc = avcodec_send_frame(rec->encoder, frame)) < 0)
        return rc;
    return write_packets(rec);
}

/* Converts count input samples (NULL to drain) into the fifo. */
static int convert(piano_recorder *rec, const float *samples, int count) {
    const int room = swr_get_out_samples(rec->swr, count);
    if (room <= 0)
        return 0;
    if (room > rec->converted_capacity) {
        if (rec->converted)
            av_freep(&rec->converted[0]);
        av_freep(&rec->converted);
        int rc = av_samples_alloc_array_and_samples(&rec->converted, NULL,
                                                    rec->encoder->ch_layout.nb_channels, room,
                                                    rec->encoder->sample_fmt, 0);
        if (rc < 0)
            return rc;
        rec->converted_capacity = room;
    }
    const uint8_t *in = (const uint8_t *)samples;
    const int got = swr_convert(rec->swr, rec->converted, room, samples ? &in : NULL, count);
    if (got < 0)
        return got;
    return av_audio_fifo_write(rec->fifo, (void **)rec->converted, got) < got ? AVERROR(ENOMEM)
                                                                              : 0;
}

static void recorder_free(piano_recorder *rec) {
    if (!rec)
        return;
    if (rec->format && !(rec->format->oformat->flags & AVFMT_NOFILE))
        avio_closep(&rec->format->pb);
    avformat_free_context(rec->format);
    avcodec_free_context(&rec->encoder);
    swr_free(&rec->swr);
    if (rec->fifo)
        av_audio_fifo_free(rec->fifo);
    if (rec->converted)
        av_freep(&rec->converted[0]);
    av_freep(&rec->converted);
    av_frame_free(&rec->frame);
    av_packet_free(&rec->packet);
    free(rec);
}

piano_recorder *piano_recorder_open(const char *path, int sample_rate, char *err,
                                    size_t err_size) {
    piano_recorder *rec = calloc(1, sizeof *rec);
    if (!rec) {
        set_error(err, err_size, "out of memory", 0);
        return NULL;
    }
    rec->sample_rate = sample_rate;
    int rc = avformat_alloc_output_context2(&rec->format, NULL, NULL, path);
    if (rc < 0 || !rec->format) {
        set_error(err, err_size, "no format for this file name", rc);
        goto fail;
    }
    const enum AVCodecID id = rec->format->oformat->audio_codec;
    const AVCodec *codec = id == AV_CODEC_ID_NONE ? NULL : avcodec_find_encoder(id);
    if (!codec) {
        set_error(err, err_size, "no audio encoder for this format", 0);
        goto fail;
    }
    rec->encoder = avcodec_alloc_context3(codec);
    rec->frame = av_frame_alloc();
    rec->packet = av_packet_alloc();
    if (!rec->encoder || !rec->frame || !rec->packet) {
        set_error(err, err_size, "out of memory", 0);
        goto fail;
    }
    AVCodecContext *enc = rec->encoder;
    enc->sample_fmt = pick_format(enc, codec);
    enc->sample_rate = pick_rate(enc, codec, sample_rate);
    enc->ch_layout = (AVChannelLayout)AV_CHANNEL_LAYOUT_MONO;
    enc->time_base = (AVRational){1, enc->sample_rate};
    enc->bit_rate = 160000; /* used by the lossy codecs only */
    if (rec->format->oformat->flags & AVFMT_GLOBALHEADER)
        enc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    if ((rc = avcodec_open2(enc, codec, NULL)) < 0) {
        set_error(err, err_size, "opening the encoder", rc);
        goto fail;
    }
    rec->frame_size = (codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE) ||
                              enc->frame_size <= 0
                          ? 4096
                          : enc->frame_size;

    rec->stream = avformat_new_stream(rec->format, NULL);
    if (!rec->stream ||
        (rc = avcodec_parameters_from_context(rec->stream->codecpar, enc)) < 0) {
        set_error(err, err_size, "adding the audio stream", rec->stream ? rc : AVERROR(ENOMEM));
        goto fail;
    }
    rec->stream->time_base = enc->time_base;

    const AVChannelLayout mono = AV_CHANNEL_LAYOUT_MONO;
    rc = swr_alloc_set_opts2(&rec->swr, &enc->ch_layout, enc->sample_fmt, enc->sample_rate,
                             &mono, AV_SAMPLE_FMT_FLT, sample_rate, 0, NULL);
    if (rc < 0 || (rc = swr_init(rec->swr)) < 0) {
        set_error(err, err_size, "setting up the resampler", rc);
        goto fail;
    }
    rec->fifo = av_audio_fifo_alloc(enc->sample_fmt, enc->ch_layout.nb_channels, rec->frame_size);
    if (!rec->fifo) {
        set_error(err, err_size, "out of memory", 0);
        goto fail;
    }

    if (!(rec->format->oformat->flags & AVFMT_NOFILE) &&
        (rc = avio_open(&rec->format->pb, path, AVIO_FLAG_WRITE)) < 0) {
        set_error(err, err_size, path, rc);
        goto fail;
    }
    if ((rc = avformat_write_header(rec->format, NULL)) < 0) {
        set_error(err, err_size, "writing the header", rc);
        goto fail;
    }
    rec->header_written = true;
    return rec;

fail:
    recorder_free(rec);
    return NULL;
}

bool piano_recorder_write(piano_recorder *rec, const float *samples, int count) {
    if (count <= 0)
        return true;
    if (convert(rec, samples, count) < 0)
        return false;
    rec->written += count;
    while (av_audio_fifo_size(rec->fifo) >= rec->frame_size)
        if (encode(rec, rec->frame_size) < 0)
            return false;
    return true;
}

double piano_recorder_seconds(const piano_recorder *rec) {
    return (double)rec->written / rec->sample_rate;
}

bool piano_recorder_close(piano_recorder *rec) {
    if (!rec)
        return false;
    bool ok = convert(rec, NULL, 0) >= 0;
    while (ok && av_audio_fifo_size(rec->fifo) >= rec->frame_size)
        ok = encode(rec, rec->frame_size) >= 0;
    int left = av_audio_fifo_size(rec->fifo);
    if (ok && left > 0) {
        /* A codec with a fixed frame size takes the last one padded with silence. */
        const bool small_last = rec->encoder->codec->capabilities &
                                (AV_CODEC_CAP_SMALL_LAST_FRAME | AV_CODEC_CAP_VARIABLE_FRAME_SIZE);
        if (!small_last) {
            uint8_t **silence = NULL;
            const int pad = rec->frame_size - left;
            ok = av_samples_alloc_array_and_samples(&silence, NULL,
                                                    rec->encoder->ch_layout.nb_channels, pad,
                                                    rec->encoder->sample_fmt, 0) >= 0;
            if (ok) {
                av_samples_set_silence(silence, 0, pad, rec->encoder->ch_layout.nb_channels,
                                       rec->encoder->sample_fmt);
                ok = av_audio_fifo_write(rec->fifo, (void **)silence, pad) == pad;
                av_freep(&silence[0]);
            }
            av_freep(&silence);
            left = rec->frame_size;
        }
        ok = ok && encode(rec, left) >= 0;
    }
    /* Drain the encoder, then finish the file. */
    ok = ok && avcodec_send_frame(rec->encoder, NULL) >= 0 && write_packets(rec) >= 0;
    if (rec->header_written)
        ok = av_write_trailer(rec->format) >= 0 && ok;
    recorder_free(rec);
    return ok;
}
