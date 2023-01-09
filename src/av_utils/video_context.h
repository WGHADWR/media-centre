//
// Created by cyy on 2023/1/4.
//

#ifndef VIDEOPLAYER_VIDEO_CONTEXT_H
#define VIDEOPLAYER_VIDEO_CONTEXT_H

#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"
#include "libavutil/avutil.h"
#include "libavutil/time.h"
#include "libavutil/opt.h"


typedef struct VideoContext {
    const char* file;
    AVFormatContext *formatContext;
    AVDictionary *options;

    AVCodecContext *videoCodecContext;
    AVCodecContext *audioCodecContext;
    AVStream *videoStream;
    AVStream *audioStream;

    int64_t duration;
} VideoContext;

static VideoContext* vc_alloc_context() {
    VideoContext *videoContext = (VideoContext *)malloc(sizeof(VideoContext));
    memset(videoContext, 0, sizeof(VideoContext));
    return videoContext;
}

static int vc_open_input(VideoContext *vc, const char* file) {
    //VideoContext *vc = (VideoContext *)malloc(sizeof(VideoContext));
    vc->file = file;


    AVDictionary *options = nullptr;
    AVFormatContext *formatContext = avformat_alloc_context();
    int ret = avformat_open_input(&formatContext, file, nullptr, &options);
    if (ret != 0) {
        av_log(nullptr, AV_LOG_ERROR, "avformat_open_input failed %d, %s\n", ret, av_err2str(ret));
        return ret;
    }
    ret = avformat_find_stream_info(formatContext, nullptr);
    vc->formatContext = formatContext;
    vc->options = options;
    vc->duration = formatContext->duration;

    ret = av_find_best_stream(formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (ret >= 0) {
        vc->videoStream = formatContext->streams[ret];
    }

    ret = av_find_best_stream(formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (ret >= 0) {
        vc->audioStream = formatContext->streams[ret];
    }

    if (vc->videoStream) {
        const AVCodec *vCodec = avcodec_find_decoder(vc->videoStream->codecpar->codec_id);
        AVCodecContext *vCodecContext = avcodec_alloc_context3(vCodec);
        vc->videoCodecContext = vCodecContext;
        ret = avcodec_open2(vCodecContext, vCodec, nullptr);
        if (ret < 0) {
            av_log(nullptr, AV_LOG_ERROR, "avcodec_open2 failed %d, %s\n", ret, av_err2str(ret));
            return ret;
        }
    }

    if (vc->audioStream) {
        const AVCodec *aCodec = avcodec_find_decoder(vc->audioStream->codecpar->codec_id);
        AVCodecContext *aCodecContext = avcodec_alloc_context3(aCodec);
        vc->audioCodecContext = aCodecContext;
        ret = avcodec_open2(aCodecContext, aCodec, nullptr);
        if (ret < 0) {
            av_log(nullptr, AV_LOG_ERROR, "avcodec_open2 failed %d, %s\n", ret, av_err2str(ret));
            return ret;
        }
    }

    return 0;
}

static AVStream* vc_new_stream(AVFormatContext *fmtContext, AVCodecID codecId, AVStream *inputStream) {
    const AVCodec *codec = avcodec_find_encoder(codecId);
    AVStream *stream = avformat_new_stream(fmtContext, codec);
    stream->index = fmtContext->nb_streams - 1;
    if (inputStream) {
        avcodec_parameters_copy(stream->codecpar, inputStream->codecpar);
        stream->time_base = { inputStream->time_base.num, inputStream->time_base.den };
    }
    return stream;
}

static int vc_open_writer(AVFormatContext *s, const char *url) {
    int ret = avio_open(&s->pb, url, AVIO_FLAG_WRITE);
    if (ret < 0) {
        printf("avio_open error %d\n", ret);
        return ret;
    }

    AVDictionary *options = nullptr;
    av_dict_set_int(&options, "video_track_timescale", 25, 0);
    ret = avformat_write_header(s, &options);
    if (ret < 0) {
        printf("avformat_write_header error %d\n", ret);
        return ret;
    }
    return 0;
}

static int vc_free_context(VideoContext *vc) {
    avcodec_free_context(&vc->videoCodecContext);
    avcodec_free_context(&vc->audioCodecContext);

    avformat_close_input(&vc->formatContext);
    avformat_free_context(vc->formatContext);

    free(vc);
    return 0;
}

#endif //VIDEOPLAYER_VIDEO_CONTEXT_H
