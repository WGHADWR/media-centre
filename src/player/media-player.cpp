
#include "media-player.h"

tm* getLocalTime() {
    time_t now;
    time(&now);
    tm* p;
    p = localtime(&now);
    return p;
}

int MediaPlayer::init(const char* url) {
    this->url = url;

    int ret = SDL_Init(SDL_INIT_EVERYTHING);
    if (ret != 0) {
        sm_log("Sdl init error: %d\n", ret);
        return ret;
    }
    sm_log("Sdl initialed\n");

    AudioSpec spec{};
    spec.out_sample_fmt = AV_SAMPLE_FMT_S16;
    spec.out_nb_channels = 2;
    spec.out_ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    spec.out_nb_samples = 1024;
    spec.out_sample_rate = 44100;
    this->audioSpec = spec;

    auto *pAudioState = new AudioState;
    pAudioState->queue = new AVPacketQueue;
    this->audioState = pAudioState;
    this->aSwrContext = nullptr;

    auto *pVideoState = new VideoState;
    pVideoState->queue = new AVPacketQueue;
    pVideoState->queue->serial = 1000000;
    this->videoState = pVideoState;

    this->audio_buf = nullptr;
    this->audio_buf_size = 0;
    //this->audio_buf_pos = nullptr;
    //this->audio_size = 0;

    //int n = 2 << av_log2(spec.out_sample_rate / 30);

    this->sync_clock = new Clock;

    return 0;
}


void initVideoCodecContext(MediaPlayer *player) {
    AVStream *stream = player->formatContext->streams[player->video_stream_index];
    const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (codec == nullptr) {
        sm_log("Cannot find video decoder\n");
        return;
    }
    AVCodecContext *codecContext = avcodec_alloc_context3(codec);
    int ret = avcodec_parameters_to_context(codecContext, stream->codecpar);
    if (ret < 0) {
        sm_log("avcodec_parameters_to_context error, %d\n", ret);
        return;
    }
    ret = avcodec_open2(codecContext, codec, nullptr);
    if (ret != 0) {
        sm_log("avcodec_open2 error, %d\n", ret);
        return;
    }

    sm_log("pixel format: %d\n", codecContext->pix_fmt);

//    SwsContext *swsContext = sws_getContext(stream->codecpar->width, stream->codecpar->height, codecContext->pix_fmt,
//                   player->width, player->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, nullptr, nullptr, nullptr);

    player->vCodecContext = codecContext;
    player->videoState->stream = stream;
    player->videoState->avg_frame_rate = &stream->avg_frame_rate;
    player->videoState->fps = av_q2d(stream->avg_frame_rate);

    sm_log("fps: %f, %f\n", player->videoState->fps, 1000 / player->videoState->fps);
    sm_log("Video decoder: %s, size: %dx%d, rate: %d, ch: %d\n",
           avcodec_get_name(stream->codecpar->codec_id),
           stream->codecpar->width, stream->codecpar->height,
           stream->codecpar->sample_rate, stream->codecpar->ch_layout.nb_channels);
}

int initAudioCodecContext(MediaPlayer *player) {
    AVStream *audioStream = player->formatContext->streams[player->audio_stream_index];
    AVCodecID avCodecId = audioStream->codecpar->codec_id;
    const AVCodec *auDecoder = avcodec_find_decoder(avCodecId);
    sm_log("Audio decoder: %s, %s\n", auDecoder->name, auDecoder->long_name);

    AVCodecContext *aCodecContext = avcodec_alloc_context3(auDecoder);
    avcodec_parameters_to_context(aCodecContext, audioStream->codecpar);
    int ret = avcodec_open2(aCodecContext, auDecoder, nullptr);
    if (ret != 0) {
        sm_log("avcodec_open2 error, %d", ret);
        return ret;
    }
    player->aCodecContext = aCodecContext;
    player->audioState->stream = audioStream;
    return 0;
}


int MediaPlayer::open(const char* file, AVDictionary **options) {
    AVFormatContext *formatContext = avformat_alloc_context();
    int ret = avformat_open_input(&formatContext, file, nullptr, options);
    if (ret != 0) {
        sm_log("Open input %s failed, %d\n", file, ret);
        ret = avformat_open_input(&formatContext, file, nullptr, options);
        if (ret != 0) {
            sm_log("Open input %s failed, retry, %d\n", file, ret);
            return ret;
        }
    }

    ret = avformat_find_stream_info(formatContext, options);
    if (ret < 0) {
        sm_log("avformat_find_stream_info failed, %d\n", ret);
        return ret;
    }

    sm_log("Duration: %lld\n", formatContext->duration);

    int videoStreamIndex = av_find_best_stream(formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (videoStreamIndex == -1) {
        sm_log("Cannot find video stream, %d\n", videoStreamIndex);
        return -1;
    }
    AVStream *videoStream = formatContext->streams[videoStreamIndex];
    this->video_stream_index = videoStreamIndex;

    int audioStreamIndex = av_find_best_stream(formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audioStreamIndex == -1) {
        sm_log("Cannot find audio stream, %d\n", audioStreamIndex);
        // return -1;
    }
    this->audio_stream_index = audioStreamIndex;

    AVRational frame_rate = av_guess_frame_rate(formatContext, videoStream, nullptr);
    double duration = (frame_rate.num && frame_rate.den ? av_q2d((AVRational){frame_rate.den, frame_rate.num}) : 0);
    double fps = av_q2d(frame_rate);
    sm_log("Video frame rate: %f, fps: %f\n", duration, fps);

    this->srcWidth = videoStream->codecpar->width;
    this->srcHeight = videoStream->codecpar->height;

    this->formatContext = formatContext;

    int w, h;
    SDL_GetWindowSize(this->window, &w, &h);
    h = w * this->srcHeight / this->srcWidth;
    this->texture = SDL_CreateTexture(this->renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, w, height);

    initVideoCodecContext(this);
    initAudioCodecContext(this);

    SDL_CreateThread(reinterpret_cast<SDL_ThreadFunction>(play), "media_play_thread", this);

//    int *status;
//    SDL_WaitThread(audio_thread, status);

    return 0;
}

void audio_callback(void *data, Uint8 *stream, int len) {
    auto *player = (MediaPlayer*)data;
    if (player->audio_buf_size == 0) {
        return;
    }
    int current_pos = 0;
    int audio_buf_size = player->audio_buf_size;
    //while (len > 0) {
        int len1 = audio_buf_size;// - current_pos;
        if (len1 < len) {
            len1 = len;
        }
        SDL_memset(stream, 0, len1);
        SDL_MixAudio(stream, player->audio_buf, len1, player->volume);
        //stream += len1;
        //current_pos += len1;
        player->audio_buf_size = 0;

        // len -= len1;
        // player->audio_size += len1;
    //}


//    double pts = audio_size / (player->audioSpec.out_sample_rate * av_get_bytes_per_sample(player->audioSpec.out_sample_fmt) * player->audioSpec.out_nb_channels);
//    SDL_Log("PTS: %f\n", pts);

    // int64_t time_rel = av_gettime_relative();
    // SDL_Log("time rel: %lld, %lld\n", time_rel, audio_size);
}

void audio_packet_queue_put(MediaPlayer *player, AVPacket *pkt) {
    player->audioState->queue->put(pkt);
}

void video_packet_queue_put(MediaPlayer *player, AVPacket *pkt) {
    player->videoState->queue->put(pkt);
}

void decode_audio_packet(void *data) {
    auto *player = (MediaPlayer*)data;

    if (player->audioState->queue->len < 20) {
        SDL_Delay(200);
    }
    //auto *outBuffer = (uint8_t *)av_malloc(MAX_AUDIO_FRAME_SIZE * player->audioSpec.out_nb_channels);

    int out_buffer_size = av_samples_get_buffer_size(
            nullptr, player->audioSpec.out_nb_channels,
            player->audioSpec.out_nb_samples, player->audioSpec.out_sample_fmt, 0);
//    SDL_Log("Sample buffer size: %d", out_buffer_size);

    AVFrame *outFrame = av_frame_alloc();
    outFrame->format = player->audioSpec.out_sample_fmt;
    outFrame->sample_rate = player->audioSpec.out_sample_rate;
    AVChannelLayout outLayout;
    av_channel_layout_default(&outLayout, player->audioSpec.out_nb_channels);
    outFrame->ch_layout = outLayout;
    outFrame->nb_samples = player->audioSpec.out_nb_samples;

    int ret0 = av_frame_get_buffer(outFrame, 0);
    if (ret0 < 0) {
        sm_log("av_frame_get_buffer error, %d\n", ret0);
        return;
    }
    uint8_t **outBuffer = outFrame->extended_data;

    player->running = 1;
    while (!player->quit) {
        AVPacketQueueItem *item = player->audioState->queue->take();
        if (item == nullptr) {
            SDL_Delay(1);
            continue;
        }
        AVPacket *pkt = item->packet;
        int ret = avcodec_send_packet(player->aCodecContext, pkt);
        if (ret != 0) {
            sm_log("avcodec_send_packet error, %d\n", ret);
            // return;
            continue;
        }
//        double sec = pkt->pts * av_q2d(player->audioState->stream->time_base);
//        player->audio_time = pkt->pts;
//        player->audioState->clock.sec = sec;

        AVFrame *frame = av_frame_alloc();
        //uint8_t *outBuffer = nullptr;
        while (avcodec_receive_frame(player->aCodecContext, frame) >= 0) {
            int out_count = frame->nb_samples * player->audioSpec.out_sample_rate / frame->sample_rate + 256;
            int out_size  = av_samples_get_buffer_size(nullptr, player->audioSpec.out_nb_channels, out_count,
                                                       player->audioSpec.out_sample_fmt, 0);
            //outBuffer = (uint8_t *)av_malloc(out_size);// * player->audioSpec.out_nb_channels);
            //unsigned int buffer_size = 0;
            //av_fast_malloc(&outBuffer, &buffer_size, out_size);
            /*if (player->audioSpec.out_nb_samples != frame->nb_samples) {
                swr_set_compensation(player->aSwrContext,
                                     (player->audioSpec.out_nb_samples - frame->nb_samples) *
                                     player->audioSpec.out_sample_rate / frame->sample_rate,
                                     player->audioSpec.out_nb_samples * player->audioSpec.out_sample_rate /
                                     frame->sample_rate
                );
            }*/

            int nb_sample_out = swr_convert(player->aSwrContext, outFrame->data, outFrame->nb_samples,
                                            (const uint8_t **)frame->extended_data, frame->nb_samples);

            /*if (nb_sample_out != out_count) {
                av_log(NULL, AV_LOG_WARNING, "audio buffer is probably too small\n");
            }*/
            int resample_size = nb_sample_out * player->audioSpec.out_nb_channels *
                             av_get_bytes_per_sample(player->audioSpec.out_sample_fmt);

            //double timeSec = av_q2d(player->aCodecContext->time_base) * frame->pts;

//            uint64_t time_rel = av_gettime_relative();
//            tm *p = getLocalTime();
            // sm_log("Audio pts: %lld, sec: %f, rel: %lld, now: %d:%d\n", frame->pts, timeSec, time_rel, p->tm_min, p->tm_sec);
//            player->audio_time = frame->pts;

            double_t pts = frame->pts == AV_NOPTS_VALUE ? 0 : frame->pts * av_q2d(player->audioState->stream->time_base);
            player->sync_clock->setClock(pts);

            player->audio_buf = *outFrame->data;
            player->audio_buf_size = resample_size; // out_buffer_size;
            // player->audio_buf_pos = player->audio_buf;

            // int size = av_samples_get_buffer_size(0,
            //                                      player->audioSpec.out_nb_channels,
            //                                      nb_sample_out, player->audioSpec.out_sample_fmt, 1);
//            int t = (player->audioSpec.out_sample_rate * 2 * 16 / 8) / size;
            // SDL_Log("buffer len: %d", audio_buf_size);
            while (player->audio_buf_size > 0) {
                SDL_Delay(1);
            }

            av_frame_unref(frame);
        }
        delete(item);
    }
    swr_free(&player->aSwrContext);
    avcodec_free_context(&player->aCodecContext);
}

void decode_video_frames(void *data) {
    auto *player = static_cast<MediaPlayer *>(data);

    int w, h;
    SDL_GetWindowSize(player->window, &w, &h);
    int height = w * player->srcHeight / player->srcWidth;

    int y = 0;
    if (h > height) {
        y = (h - height) / 2;
    }

    SDL_Rect rect;
    rect.x = 0;
    rect.y = y;
    rect.w = w;
    rect.h = height;

    SDL_Rect srcRect;
    srcRect.x = 0;
    srcRect.y = 0;
    srcRect.w = w;
    srcRect.h = height;


    AVFrame *frame = av_frame_alloc();
    AVFrame *dest = av_frame_alloc();
    dest->format = AV_PIX_FMT_YUV420P;
    dest->width = w;
    dest->height = height;
    av_frame_get_buffer(dest, 1);

    SwsContext *swsContext = nullptr; // = sws_alloc_context();

    while (!player->quit) {
        if (!player->running) {
            SDL_Delay(3);
            continue;
        }
        AVPacketQueueItem *item = player->videoState->queue->take();
        if (item == nullptr) {
            SDL_Delay(20);
            continue;
        }
//         SDL_Log("video pkt: %d\n", item->id);
        AVPacket *pkt = item->packet;
        int ret = avcodec_send_packet(player->vCodecContext, pkt);
        if (ret != 0) {
            sm_log("avcodec_send_packet error, %d\n", ret);
            continue;
        }
        //sm_log("Pkt, pts: %lld, du: %lld\n", pkt->pts, pkt->duration);
        while ((ret = avcodec_receive_frame(player->vCodecContext, frame)) == 0) {
            if (swsContext == nullptr) {
                swsContext = sws_getContext(
                        frame->width, frame->height,
                        static_cast<AVPixelFormat>(frame->format),
                        dest->width, dest->height, static_cast<AVPixelFormat>(dest->format), SWS_BICUBIC, nullptr, nullptr, nullptr);

            }

            int output_slice = sws_scale(swsContext, frame->data, frame->linesize, 0, frame->height,
                                         dest->data, dest->linesize);
            if (output_slice == 0) {
                sm_log("sws_scale error\n");
                continue;
            }

            double time = av_q2d(player->videoState->stream->time_base) * frame->pts;
            double_t diff = time - player->sync_clock->getClock();
            double extra = frame->repeat_pict / (2 * player->videoState->fps);
            double fps_delay = 1.0 / player->videoState->fps;
            double real_delay = extra + fps_delay;

            sm_log("Video pts: %lld, time: %f, aClock: %f, diff: %f, real_delay: %f, d_r: %f, duration: %lld\n",
                    frame->pts, time, player->sync_clock->getClock(), diff, real_delay, diff + real_delay * AV_TIME_BASE, frame->duration);

            /*if (diff + real_delay > 0) {
                av_usleep((diff + real_delay) * AV_TIME_BASE);
            }*/
            if (diff > 0) {
                //sm_log("sleep %f\n", diff + real_delay * AV_TIME_BASE);
                av_usleep( diff + real_delay * AV_TIME_BASE);
                //av_usleep(diff);
            } else {
                /*if (fabs(diff) > 0.05) {
                    sm_log("drop frame\n");
                    continue;
                }*/
            }

            SDL_UpdateYUVTexture(player->texture, nullptr,
                                 dest->data[0], dest->linesize[0],
                                 dest->data[1], dest->linesize[1],
                                 dest->data[2], dest->linesize[2]);

            SDL_SetRenderDrawColor(player->renderer, 0, 0, 0, 255);
            SDL_RenderClear(player->renderer);
            SDL_RenderCopy(player->renderer, player->texture, &srcRect, &rect);
            SDL_RenderPresent(player->renderer);
        }
        if (ret == AVERROR(EAGAIN)) {

        } else {
            sm_log("avcodec_receive_frame error, %d\n", ret);
        }
        delete item;
        if (player->force_refresh == 1) {
            SDL_SetRenderDrawColor(player->renderer, 0, 0, 0, 255);
            SDL_RenderClear(player->renderer);
            SDL_RenderPresent(player->renderer);

            player->force_refresh = 0;
        }

        // SDL_Delay(50);
    }
    av_frame_free(&dest);
    av_frame_free(&frame);
}

int MediaPlayer::play(void *data)
{
    auto *player = (MediaPlayer*)data;
    //player->audioSpec.out_nb_channels = audioStream->codecpar->ch_layout.nb_channels;
//    int out_nb_channels = 2;
//    int out_samples = 1024;
//    int out_sample_rate = 44100;
//    const AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
    AVChannelLayout ch_layout_out;
    av_channel_layout_default(&ch_layout_out, player->audioSpec.out_nb_channels);

    AVChannelLayout ch_layout_in;
    av_channel_layout_copy(&ch_layout_in, &player->aCodecContext->ch_layout);
    AVSampleFormat sample_fmt_in = player->aCodecContext->sample_fmt;
    int sample_rate_in = player->aCodecContext->sample_rate;


    player->audioSpec.out_sample_rate = player->aCodecContext->sample_rate;
    player->audioSpec.out_sample_rate = player->audioSpec.out_sample_rate > player->aCodecContext->sample_rate ?
                                        player->audioSpec.out_sample_rate : player->aCodecContext->sample_rate;

    SDL_AudioSpec wanted_spec, spec;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.channels = player->audioSpec.out_nb_channels;
    wanted_spec.samples = player->audioSpec.out_nb_samples;
    wanted_spec.freq = player->audioSpec.out_sample_rate;
    wanted_spec.callback = audio_callback;
    wanted_spec.userdata = player;

    SDL_Log("SDL_AudioSpec, format: %d, channels: %d, samples: %d, freq: %d\n",
           wanted_spec.format, wanted_spec.channels, wanted_spec.samples, wanted_spec.freq);

    SDL_Log("Input_AudioSpec, format: %d, channels: %d, sample fmt: %s, sample rate: %d\n",
            sample_fmt_in, ch_layout_in.nb_channels, av_get_sample_fmt_name(sample_fmt_in), sample_rate_in);

    SDL_AudioDeviceID deviceId;
//    while (!(deviceId = (SDL_OpenAudioDevice(NULL, 0, &wanted_spec, &spec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE)))) {
//        av_log(NULL, AV_LOG_WARNING, "SDL_OpenAudio (%d channels, %d Hz): %s\n",
//               wanted_spec.channels, wanted_spec.freq, SDL_GetError());
//    }

    int ret;
    if ((ret = SDL_OpenAudio(&wanted_spec, nullptr)) != 0) {
        sm_log("SDL_OpenAudio error, %d\n", ret);
        return ret;
    }

    SwrContext *swrContext = swr_alloc();
    ret = swr_alloc_set_opts2(&swrContext,
                              &ch_layout_out, player->audioSpec.out_sample_fmt, player->audioSpec.out_sample_rate,
                        &ch_layout_in, sample_fmt_in, sample_rate_in,
                        0, nullptr);

    if (ret != 0) {
        sm_log("swr_alloc_set_opts2 error, %d\n", ret);
        return ret;
    }
    ret = swr_init(swrContext);
    if (ret != 0) {
        sm_log("swr_init error, %d\n", ret);
        return ret;
    }
    player->aSwrContext = swrContext;

    SDL_PauseAudio(0);

    SDL_CreateThread(reinterpret_cast<SDL_ThreadFunction>(decode_audio_packet), "audio_decode_thread", player);

    SDL_CreateThread(reinterpret_cast<SDL_ThreadFunction>(decode_video_frames), "video_decode_thread", player);

//    int out_buffer_size = av_samples_get_buffer_size(
//            nullptr, out_nb_channels, out_samples, out_sample_fmt, 1);
    // uint8_t *outBuffer = (uint8_t *)av_malloc(MAX_AUDIO_FRAME_SIZE * this->audioSpec.out_nb_channels);

    AVPacket *pkt = av_packet_alloc();
    for (;;) {
        ret = av_read_frame(player->formatContext, pkt);
        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                break;
            }
            if (pkt && ret != AVERROR(EAGAIN)) {
                continue;
            } else {
                break;
            }
        }
        if (pkt->stream_index == player->audio_stream_index) {
            audio_packet_queue_put(player, pkt);
        } else if (pkt->stream_index == player->video_stream_index) {
            video_packet_queue_put(player, pkt);
        }
        av_packet_unref(pkt);
    }
    return 0;
}

void on_window_resized(SDL_Event *event, MediaPlayer *player, SDL_Window *window) {
    sm_log("Window size, w: %d, h: %d\n", event->window.data1, event->window.data2);
}

int MediaPlayer::createWindow() {
    SDL_Window *window = SDL_CreateWindow("Player", 100, 100, width, height,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    this->renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    // this->texture = SDL_CreateTexture(this->renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, width, height);
    this->window = window;
    SDL_ShowWindow(window);

    SDL_Event event;
    while (SDL_WaitEvent(&event)) {
        if (event.type == SDL_QUIT) {
            this->quit = 1;
            return 0;
        }
        if (event.type == SDL_WINDOWEVENT) {
            if (event.window.event == SDL_WINDOWEVENT_SHOWN) {
                this->open(url, nullptr);
            }
            if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                on_window_resized(&event, this, window);
            }
            if (event.window.event == SDL_WINDOWEVENT_EXPOSED) {
                this->force_refresh = 1;
            }
            sm_log("SDL event, %d\n", event.window.type);
        }
        if (event.type == SDL_KEYUP) {
            sm_log("Press key: %d\n, space: %d", event.key.keysym.sym, event.key.keysym.sym == SDLK_SPACE);

            if (event.key.keysym.sym == SDLK_SPACE) {
                //this->audioState->pause = this->audioState->pause == 0 ? 1 : 0;
                //SDL_PauseAudio(this->audioState->pause);
                this->volume = this->volume == SDL_MIX_MAXVOLUME ? 0 : SDL_MIX_MAXVOLUME;
            }

        }
    }

    return 0;
}

void MediaPlayer::free() const {
    avformat_free_context(this->formatContext);

    SDL_DestroyTexture(this->texture);
    SDL_DestroyRenderer(this->renderer);
    SDL_DestroyWindow(this->window);
}

MediaPlayer::~MediaPlayer() {
    this->free();
}
