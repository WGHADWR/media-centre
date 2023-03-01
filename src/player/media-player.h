//
// Created by cyy on 2022/11/21.
//

#ifndef VIDEOPLAYER_MEDIA_PLAYER_H
#define VIDEOPLAYER_MEDIA_PLAYER_H

#ifdef __cplusplus
extern "C"
{
#endif
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"
#include "libavutil/avutil.h"
#include "libavutil/time.h"

#include "SDL2/SDL.h"
#ifdef __cplusplus
}
#endif

#include "../av_utils/sm_log.h"
#include "../av_utils/av_utils.h"
#include "../av_utils/av_packet_queue.h"

#define MAX_AUDIO_FRAME_SIZE 192000

struct AudioSpec {
    int out_nb_channels;
    int out_nb_samples;
    int out_sample_rate;
    AVSampleFormat out_sample_fmt;
    AVChannelLayout out_ch_layout;
};

struct AudioState {
    AVPacketQueue *queue;
    AVStream *stream;

    int pause = 0;
};

struct VideoState {
    AVPacketQueue *queue;

    AVStream *stream;
    AVRational *avg_frame_rate;
    double_t fps;
};


class MediaPlayer {

protected:
    const char* url = nullptr;

public:
    uint8_t *audio_buf;
    unsigned int audio_buf_size = 0;
    unsigned int audio_buf_pos = 0;

    unsigned int audio_size;

    AudioSpec audioSpec;

    AVFormatContext *formatContext;
    int video_stream_index;
    int audio_stream_index;

    AudioState *audioState;
    VideoState *videoState;

    AVCodecContext *vCodecContext;
//    SwsContext *swsContext;
    AVCodecContext *aCodecContext;
    struct SwrContext *aSwrContext;

    int64_t audio_time;

    int srcWidth;
    int srcHeight;

    int width = 600;
    int height = 400;

    int volume = SDL_MIX_MAXVOLUME;

    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    int force_refresh = 0;

    Clock *sync_clock;

    int quit = 0;
    int running = 0;

    int init(const char* url);

    int open(const char* file, AVDictionary **options);

    static int play(void *data);

    int createWindow();

    void free() const;

    ~MediaPlayer();
};

#endif //VIDEOPLAYER_MEDIA_PLAYER_H
