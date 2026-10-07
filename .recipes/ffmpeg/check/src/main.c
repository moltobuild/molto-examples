/* What a consumer of the recipe sees: every library links, the version is
   the one pinned, the components are all there, and the decoder runs on the
   fastest code this CPU has. */
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavutil/cpu.h>
#include <libavutil/ffversion.h>
#include <libavutil/hwcontext.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>

#include <stdio.h>

static int count_codecs(int (*is)(const AVCodec *)) {
    int count = 0;
    void *at = NULL;
    for (const AVCodec *codec = av_codec_iterate(&at); codec != NULL; codec = av_codec_iterate(&at))
        count += is(codec) != 0;
    return count;
}

static int count_demuxers(void) {
    int count = 0;
    void *at = NULL;
    while (av_demuxer_iterate(&at) != NULL)
        count++;
    return count;
}

static int count_filters(void) {
    int count = 0;
    void *at = NULL;
    while (av_filter_iterate(&at) != NULL)
        count++;
    return count;
}

int main(void) {
    avdevice_register_all();
    printf("FFmpeg %s\n", FFMPEG_VERSION);
    printf("decoders %d, encoders %d, demuxers %d, filters %d\n",
           count_codecs(av_codec_is_decoder), count_codecs(av_codec_is_encoder), count_demuxers(),
           count_filters());
    printf("swscale %u, swresample %u\n", swscale_version(), swresample_version());
    printf("cpu flags 0x%x\n", av_get_cpu_flags());
    printf("hardware:");
    for (enum AVHWDeviceType type = av_hwdevice_iterate_types(AV_HWDEVICE_TYPE_NONE);
         type != AV_HWDEVICE_TYPE_NONE; type = av_hwdevice_iterate_types(type))
        printf(" %s", av_hwdevice_get_type_name(type));
    printf("\n");
    const AVCodec *h264 = avcodec_find_decoder(AV_CODEC_ID_H264);
    AVCodecContext *context = avcodec_alloc_context3(h264);
    const int opened = context != NULL ? avcodec_open2(context, h264, NULL) : -1;
    printf("h264 decoder %s: %s\n", h264 != NULL ? h264->name : "missing",
           opened == 0 ? "opens" : "fails");
    avcodec_free_context(&context);
    return opened == 0 ? 0 : 1;
}
