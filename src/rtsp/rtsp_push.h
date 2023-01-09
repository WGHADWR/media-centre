//
// Created by cyy on 2022/11/24.
//

#ifndef VIDEOPLAYER_RTSP_PUSH_H
#define VIDEOPLAYER_RTSP_PUSH_H

extern "C"
{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"
#include "libavutil/avutil.h"
#include "libavutil/time.h"
#include "libavutil/opt.h"

//#include "SDL2/SDL.h"

#include "string.h"
#include "ctype.h"
}

#include <cstring>
#include "../av_utils/sm_log.h"
#include "../av_utils/av_utils.h"

struct AudioCodecPar {
    AVCodecID codecId;
    int nb_channels;
    int nb_samples;
    AVSampleFormat sample_fmt;
    int sample_rate;
};

struct VideoCodecPar {
    AVCodecID codecId;
    int width;
    int height;
};

class RtspPusher {

public:
    const char* src;
    const char* url;

    int quit = 0;

    AudioCodecPar *audioCodecPar;
    VideoCodecPar *videoCodecPar;

    AVFormatContext *inputFormatContext;
    AVStream *inputVStream;
    AVStream *inputAStream;
    AVCodecContext *inputVCodecContext;
    AVCodecContext *inputACodecContext;

    AVFormatContext *formatContext;
    AVFormatContext *outFormatContext;
    AVCodecContext *videoCodecContext;
    AVCodecContext *audioCodecContext;

    const AVCodec *vCodec = nullptr;
    const AVCodec *aCodec = nullptr;
    AVStream *videoStream = nullptr;
    AVStream *audioStream = nullptr;

    SwrContext *swrContext = nullptr;

    Clock *sync_clock = nullptr;

    int64_t start_time;

    RtspPusher(const char* src, const char* target);
    int init(VideoCodecPar *vCodecPar, AudioCodecPar *aCodecPar);
    int add_video_stream(AVStream *srcStream);
    int add_audio_stream();

    void start();

    int push(AVPacket *pkt) const;

    void destroy();

    ~RtspPusher();
};


#endif //VIDEOPLAYER_RTSP_PUSH_H
