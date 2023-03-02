//
// Created by cyy on 2023/1/4.
//

#ifndef VIDEOPLAYER_VIDEO_CONTEXT_H
#define VIDEOPLAYER_VIDEO_CONTEXT_H

#ifdef __cplusplus
extern "C" {
#endif
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"
#include "libavutil/avutil.h"
#include "libavutil/time.h"
#include "libavutil/opt.h"
#include "libavutil/audio_fifo.h"
#ifdef __cplusplus
};
#endif

#include "../util/sm_log.h"
#include "av_utils.h"

typedef struct VideoContext {
    const char* file;
    AVFormatContext *inputFormatContext;
    AVDictionary *options;

    AVCodecContext *inVideoCodecContext;
    AVCodecContext *inAudioCodecContext;
    AVStream *inVideoStream;
    AVStream *inAudioStream;

    AVFormatContext *dstFormatContext;
    AVCodecContext *dstVideoCodecContext;
    AVCodecContext *dstAudioCodecContext;

    AVStream *dstVideoStream;
    AVStream *dstAudioStream;

    SwrContext *swr;

    AVAudioFifo *fifo;

    Clock *audioClk;
    Clock *videoClk;

    int64_t duration;
} VideoContext;

typedef struct SwrContextOpts {
    AVChannelLayout out_ch_layout;
    AVSampleFormat out_sample_fmt;
    int out_sample_rate;

    AVChannelLayout in_ch_layout;
    AVSampleFormat in_sample_fmt;
    int in_sample_rate;
} SwrContextOpts;

static VideoContext* vc_alloc_context() {
    VideoContext *videoContext = (VideoContext *)malloc(sizeof(VideoContext));
    memset(videoContext, 0, sizeof(VideoContext));

    videoContext->audioClk = new Clock;
    return videoContext;
}

static int vc_open_input(VideoContext *vc, const char* file) {
    //VideoContext *vc = (VideoContext *)malloc(sizeof(VideoContext));
    vc->file = file;

    AVDictionary *options = nullptr;
    AVFormatContext *formatContext = avformat_alloc_context();
    int ret = avformat_open_input(&formatContext, file, nullptr, &options);
    if (ret != 0) {
        av_log(nullptr, AV_LOG_ERROR, "avformat_open_input failed %d, %s\n", ret, av_errStr(ret));
        return ret;
    }
    ret = avformat_find_stream_info(formatContext, nullptr);
    vc->inputFormatContext = formatContext;
    vc->options = options;
    vc->duration = formatContext->duration;

    ret = av_find_best_stream(formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (ret >= 0) {
        vc->inVideoStream = formatContext->streams[ret];
    }

    ret = av_find_best_stream(formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (ret >= 0) {
        vc->inAudioStream = formatContext->streams[ret];
    }

    if (vc->inVideoStream) {
        const AVCodec *vCodec = avcodec_find_decoder(vc->inVideoStream->codecpar->codec_id);
        AVCodecContext *vCodecContext = avcodec_alloc_context3(vCodec);
        avcodec_parameters_to_context(vCodecContext, vc->inVideoStream->codecpar);
        ret = avcodec_open2(vCodecContext, vCodec, nullptr);
        if (ret < 0) {
            av_log(nullptr, AV_LOG_ERROR, "avcodec_open2 failed %d, %s\n", ret, av_errStr(ret));
            return ret;
        }
        vc->inVideoCodecContext = vCodecContext;
    }

    if (vc->inAudioStream) {
        const AVCodec *aCodec = avcodec_find_decoder(vc->inAudioStream->codecpar->codec_id);
        AVCodecContext *aCodecContext = avcodec_alloc_context3(aCodec);
        avcodec_parameters_to_context(aCodecContext, vc->inAudioStream->codecpar);
        ret = avcodec_open2(aCodecContext, aCodec, nullptr);
        if (ret < 0) {
            av_log(nullptr, AV_LOG_ERROR, "avcodec_open2 failed %d, %s\n", ret, av_errStr(ret));
            return ret;
        }
        vc->inAudioCodecContext = aCodecContext;
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

static int init_swr_context(SwrContext **swrContext, SwrContextOpts *opts) {
    if (*swrContext) {
        swr_free(swrContext);
    }
    *swrContext = swr_alloc();
    swr_alloc_set_opts2(swrContext, &opts->out_ch_layout, opts->out_sample_fmt, opts->out_sample_rate,
                        &opts->in_ch_layout, opts->in_sample_fmt, opts->in_sample_rate, 0, nullptr);

    int ret = swr_init(*swrContext);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "swr_init failed. ret: %d\n", ret);
        return ret;
    }

    char ch_layout_desc_out[512];
    char ch_layout_desc_in[512];
    av_channel_layout_describe(&opts->out_ch_layout, ch_layout_desc_out, sizeof(ch_layout_desc_out));
    av_channel_layout_describe(&opts->in_ch_layout, ch_layout_desc_in, sizeof(ch_layout_desc_in));
    std::cout << "Swr context opts:" << std::endl;
    std::cout << "      out: "
              << opts->out_sample_rate << " Hz, " << ch_layout_desc_out << ", "
              << av_get_sample_fmt_name(opts->out_sample_fmt) << "\n"
              << "      in:  "
              << opts->in_sample_rate << " Hz, " << ch_layout_desc_in << ", "
              << av_get_sample_fmt_name(opts->in_sample_fmt) << std::endl << std::endl;
    return 0;
}


static int vc_swr_resample(SwrContext *swrContext, AVFrame *frame, AVFrame *dest) {
    int64_t dst_nb_samples = av_rescale_rnd(swr_get_delay(swrContext, frame->sample_rate) + frame->nb_samples,
                                        dest->sample_rate, frame->sample_rate, AV_ROUND_UP);

    int resample_count = swr_convert(swrContext, dest->data, (int)dst_nb_samples, (const uint8_t **)(frame->extended_data), frame->nb_samples);
    if (resample_count <= 0) {
        return -1;
    }
    return resample_count;
}


static int vc_free_context(VideoContext *vc) {
    avcodec_free_context(&vc->inVideoCodecContext);
    avcodec_free_context(&vc->inAudioCodecContext);

    avformat_close_input(&vc->inputFormatContext);
    avformat_free_context(vc->inputFormatContext);

    if (vc->dstVideoCodecContext) {
        avcodec_free_context(&vc->dstVideoCodecContext);
    }
    if (vc->dstAudioCodecContext) {
        avcodec_free_context(&vc->dstAudioCodecContext);
    }
    if (vc->dstFormatContext) {
        /*if (vc->dstFormatContext->pb) {
            avio_close(vc->dstFormatContext->pb);
        }*/
        avformat_free_context(vc->dstFormatContext);
    }

    if (vc->swr) {
        swr_free(&vc->swr);
    }

    free(vc);
    return 0;
}

#endif //VIDEOPLAYER_VIDEO_CONTEXT_H
