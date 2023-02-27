//
// Created by cyy on 2022/11/24.
//

#include "rtsp_push.h"

char* str_upper(char *str) {
    if (str == nullptr) {
        return nullptr;
    }
    char *p = str;
    while (*str)
    {
        *str = toupper(*str);
        str++;
    }
    return p;
}

bool isRtsp(const char* url) {
    const char *c = strchr(url, ':');
    uint64_t i = strlen(url) - strlen(c);
    char protocol[10] = { 0 };
    strncpy(protocol, url, i);
    char* p = str_upper(protocol);
    return strcmp(p, "RTSP") == 0;
}

void audioCodecPar_copy(AudioCodecPar *dest, AudioCodecPar *src) {
    dest->codecId = src->codecId;
    dest->sample_fmt = src->sample_fmt;
    dest->sample_rate = src->sample_rate;
    dest->nb_samples = src->nb_samples;
    dest->nb_channels = src->nb_channels;
}

RtspPusher::RtspPusher(const char* src, const char* target) {
    this->src = src;
    this->url = target;

    this->outFormatContext = nullptr;
}

int newVideoCodecContext(RtspPusher *pusher, VideoCodecPar *vCodecPar) {
    AVCodecContext *codecContext = nullptr;
    AVRational time_base = //{ 1, 25 };
             { pusher->inputVStream->time_base.num, pusher->inputVStream->time_base.den };

    const AVCodec *codec = avcodec_find_encoder(vCodecPar->codecId);
    if (codec == nullptr) {
        sm_log("cannot find encoder, codecId: %d\n", vCodecPar->codecId);
        return -1;
    }
    codecContext = avcodec_alloc_context3(codec);
    if (codecContext == nullptr) {
        sm_log("avcodec_alloc_context3 error\n");
        return -1;
    }

    codecContext->codec_id = codec->id;
    codecContext->codec_type = AVMEDIA_TYPE_VIDEO;
    codecContext->pix_fmt = AV_PIX_FMT_YUV420P;
//    codecContext->bit_rate = 1000000;
    codecContext->width = vCodecPar->width;
    codecContext->height = vCodecPar->height;
    codecContext->time_base = time_base;
    codecContext->gop_size = 5;
    codecContext->max_b_frames = 0;
//    codecContext->bit_rate = 40000;
    // codecContext->framerate = time_base;
    codecContext->codec_tag = 0;
    codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (codec->id == AV_CODEC_ID_H264) {
        av_opt_set(codecContext->priv_data, "tune", "zerolatency", 0);
        av_opt_set(codecContext->priv_data, "preset", "superfast", 0);
    }

    int ret = avcodec_open2(codecContext, codec, nullptr);
    if (ret < 0) {
        sm_log("cannot open video codec context, %d\n", ret);
        return ret;
    }

    pusher->vCodec = codec;
    pusher->videoCodecContext = codecContext;

    return 0;
}

int newAudioCodecContext(RtspPusher *pusher, AudioCodecPar *codecPar) {
    AVCodecContext *codecContext = nullptr;
    AVRational time_base = //{ 1, codecPar->sample_rate };
            { 1, pusher->inputAStream->codecpar->sample_rate };

    const AVCodec *codec = avcodec_find_encoder(codecPar->codecId);
    if (codec == nullptr) {
        sm_log("cannot find encoder, codecId: %d\n", codecPar->codecId);
        return -1;
    }
    codecContext = avcodec_alloc_context3(codec);
    if (codecContext == nullptr) {
        sm_log("avcodec_alloc_context3 error\n");
        return -1;
    }

    // AVChannelLayout channelLayout;
    // av_channel_layout_default(&channelLayout, codecPar->nb_channels);
    // codecContext->ch_layout = channelLayout;
    av_channel_layout_default(&codecContext->ch_layout, codecPar->nb_channels);
    codecContext->codec_id = codec->id;
    codecContext->codec_type = AVMEDIA_TYPE_AUDIO;
    codecContext->time_base = time_base;
    codecContext->codec_tag = 0;
    codecContext->sample_rate = codecPar->sample_rate;
    codecContext->sample_fmt = codecPar->sample_fmt;
    codecContext->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
    codecContext->bit_rate = 128000;
    codecContext->frame_size = codecPar->nb_samples;
    //codecContext->profile = FF_PROFILE_AAC_HE_V2;
    codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

//    if (pusher->outFormatContext->oformat->flags & AVFMT_GLOBALHEADER) {
//        codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
//    }

    avcodec_parameters_to_context(codecContext, pusher->inputAStream->codecpar);

    pusher->aCodec = codec;
    pusher->audioCodecContext = codecContext;
    av_opt_set(pusher->audioCodecContext->priv_data, "tune", "zerolatency", 0);

    int ret = avcodec_open2(pusher->audioCodecContext, pusher->aCodec, nullptr);
    if (ret < 0) {
        sm_log("cannot open audio codec context, %d\n", ret);
        return ret;
    }
    return 0;
}

int RtspPusher::init(VideoCodecPar *vCodecPar, AudioCodecPar *aCodecPar) {
    this->videoCodecPar = vCodecPar;
    this->audioCodecPar = aCodecPar;

    this->sync_clock = new Clock;
    return 0;
}

int RtspPusher::add_video_stream(AVStream *inputStream) {
    AVCodecID codecId;
    if (inputStream == nullptr) {
        codecId = this->videoCodecPar->codecId;
    } else {
        codecId = inputStream->codecpar->codec_id;
    }
    const AVCodec *codec = avcodec_find_encoder(codecId);
    //AVRational time_base = { 1, 25 };
    this->videoStream = avformat_new_stream(this->outFormatContext, codec);
    this->videoStream->id = this->outFormatContext->nb_streams - 1;
    this->videoStream->time_base = { inputStream->time_base.num, inputStream->time_base.den } ;//time_base;
    this->videoStream->codecpar->codec_tag = 0;
//    int ret = avcodec_parameters_from_context(this->videoStream->codecpar, this->videoCodecContext);
//    if (ret < 0) {
//        av_log(nullptr, AV_LOG_ERROR, "avcodec_parameters_from_context error, %d\n", ret);
//    }
    if (inputStream != nullptr) {
        avcodec_parameters_copy(this->videoStream->codecpar, inputStream->codecpar);
    }

    //avcodec_parameters_from_context(this->videoStream->codecpar, this->videoCodecContext);

    return 0;
}

int RtspPusher::add_audio_stream() {
    this->audioStream = avformat_new_stream(this->outFormatContext, this->audioCodecContext->codec);
    this->audioStream->id = outFormatContext->nb_streams - 1;
    this->audioStream->time_base = this->audioCodecContext->time_base;
     this->audioStream->codecpar->codec_tag = 0;
    int ret = avcodec_parameters_from_context(this->audioStream->codecpar, this->audioCodecContext);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "avcodec_parameters_from_context error, %d\n", ret);
    }

    if (this->inputAStream != nullptr) {
        avcodec_parameters_copy(this->audioStream->codecpar, this->inputAStream->codecpar);
    }
    return ret;
}

/**
 * Target: H264/ACC
 * @param stream
 * @return bool
 */
bool is_target_codec(AVStream *stream) {
    return stream->codecpar->codec_id == AV_CODEC_ID_H264
        || stream->codecpar->codec_id == AV_CODEC_ID_AAC;
}

int encode_write_audio_frame(RtspPusher *pusher, AVFrame *frame) {
    int ret = avcodec_send_frame(pusher->audioCodecContext, frame);
    if (ret != 0) {
        if (ret == AVERROR(EINVAL)) {

        }
        return ret;
    }

    //const char *type_desc = av_get_media_type_string(pusher->audioCodecContext->codec_type);
    AVPacket *destPkt = av_packet_alloc();
    ret = avcodec_receive_packet(pusher->audioCodecContext, destPkt);
    if (ret < 0) {
        av_packet_free(&destPkt);
        sm_log("avcodec_receive_packet error, audio, %d, %s\n", ret, av_errStr(ret));
        /*if (ret == AVERROR(EAGAIN)) {
            av_log(nullptr, AV_LOG_ERROR, "avcodec_receive_packet EAGAIN, %d\n", ret);
        } else if (ret == AVERROR_EOF) {
            av_log(nullptr, AV_LOG_ERROR, "avcodec_receive_packet AVERROR_EOF, %d\n", ret);
        }*/
        return ret;
    }

    if (destPkt->pts == AV_NOPTS_VALUE) {
        destPkt->pts = destPkt->dts = av_gettime() - pusher->start_time;
        destPkt->duration = pusher->audioCodecPar->nb_samples;
    }

    destPkt->stream_index = pusher->audioStream->index;
    av_packet_rescale_ts(destPkt, pusher->audioCodecContext->time_base, pusher->audioStream->time_base);

    //destPkt->pts = destPkt->dts = av_rescale_q(av_gettime() - pusher->start_time, AV_TIME_BASE_Q, pusher->audioStream->time_base);

    // av_packet_rescale_ts(destPkt, pusher->inputAStream->time_base, pusher->audioStream->time_base);
    destPkt->pos = -1;
    ret = pusher->push(destPkt);
    /*if (ret < 0) {
        if (ret == AVERROR(EPIPE)) {
            return ret;
        }
    }*/
    av_packet_unref(destPkt);
    return ret;
}

int convert_audio_packet(RtspPusher *pusher, AVPacket *pkt) {
    int ret = avcodec_send_packet(pusher->inputACodecContext, pkt);
    if (ret != 0) {
        return ret;
    }
    AVFrame *frame = av_frame_alloc();

    AVFrame *dstFrame = av_frame_alloc();
    //AVChannelLayout channelLayout;
    //av_channel_layout_default(&channelLayout, pusher->audioCodecContext->ch_layout.nb_channels);
    //dstFrame->ch_layout = channelLayout;
    av_channel_layout_copy(&dstFrame->ch_layout, &pusher->audioCodecContext->ch_layout);
    dstFrame->format = pusher->audioCodecPar->sample_fmt;
    dstFrame->sample_rate = pusher->audioCodecContext->sample_rate;
    dstFrame->nb_samples = pusher->audioCodecPar->nb_samples; //pusher->audioCodecContext->frame_size;

    if (dstFrame->nb_samples) {
        ret = av_frame_get_buffer(dstFrame, 0);
        if (ret < 0) {
            sm_log("av_frame_get_buffer error, audio, %d, %s\n", ret, av_errStr(ret));
            return ret;
        }
    }

//    uint8_t **dst_data;
//    int *line_size;
//    av_samples_alloc_array_and_samples(&dst_data, line_size, pusher->audioCodecPar->nb_channels,
//                                       pusher->audioCodecPar->nb_samples, pusher->audioCodecPar->sample_fmt, 0);

    // int sample_size = pusher->audioCodecContext->frame_size * av_get_bytes_per_sample(pusher->audioCodecContext->sample_fmt);
    // sm_log("Sample size: %d\n", sample_size);

    while (true) {
        ret = avcodec_receive_frame(pusher->inputACodecContext, frame);
        if (ret < 0) {
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                // AVERROR(EAGAIN)
                break;
            }
            sm_log("avcodec_receive_frame error, %d, %s\n", ret, av_errStr(ret));
            break;
        }
        // destFrame->nb_samples = pusher->audioCodecPar->nb_samples;

        int dst_nb_samples = av_rescale_rnd(
                swr_get_delay(pusher->swrContext, pusher->audioCodecContext->sample_rate) + frame->nb_samples,
                pusher->audioCodecContext->sample_rate, pusher->audioCodecContext->sample_rate, AV_ROUND_UP);

        if (dst_nb_samples > dstFrame->nb_samples) {
            av_frame_unref(dstFrame);

            // dstFrame->ch_layout = channelLayout;
            dstFrame->format = pusher->audioCodecPar->sample_fmt;
            dstFrame->sample_rate = pusher->audioCodecContext->sample_rate;
            dstFrame->nb_samples = dst_nb_samples;
            av_frame_get_buffer(dstFrame, 0);

            sm_log("reset nb_samples, new: %d\n", dst_nb_samples);
        }
        if (pusher->audioCodecPar->nb_samples != frame->nb_samples) {
            ret = swr_set_compensation(pusher->swrContext,
                                       (pusher->audioCodecPar->nb_samples - frame->nb_samples) * pusher->audioCodecPar->sample_rate / frame->sample_rate,
                                       pusher->audioCodecPar->nb_samples * pusher->audioCodecPar->sample_rate / frame->sample_rate);
            if (ret < 0) {
                sm_log("swr_set_compensation error, %d\n", ret);
            }
        }

        // int nb_samples = dstFrame->nb_samples;
        ret = swr_convert(pusher->swrContext, dstFrame->data, dst_nb_samples,
                          (const uint8_t **)frame->extended_data, frame->nb_samples);
        if (ret < 0) {
            sm_log("swr_convert error, %d, %s\n", ret, av_errStr(ret));
            continue;
        }
        //dstFrame->nb_samples = ret;
        //dstFrame->data[0] = dst_data[0];
        //dstFrame->data[1] = dst_data[1];

        dstFrame->pts = frame->pts;
        dstFrame->pkt_dts = frame->pkt_dts;

        if (!avcodec_is_open(pusher->audioCodecContext) || !av_codec_is_encoder(pusher->audioCodecContext->codec)) {
            return AVERROR(EINVAL);
        }

        //destFrame->pts = frame->pts; // av_gettime();
        ret = encode_write_audio_frame(pusher, dstFrame);
        if (ret < 0) {
            sm_log("encode_write_audio_frame error, %d, %s\n", ret, av_errStr(ret));
            if (ret == AVERROR(EAGAIN)) {
                continue;
            }
            if (ret == AVERROR(EPIPE)) {
                return ret;
            }
            break;
        }
    }
    av_frame_free(&frame);
    return ret;
}

int RtspPusher::push(AVPacket *pkt) const {
    // sm_log("pts: %lld\n", pkt->pts);
    int ret = av_interleaved_write_frame(this->outFormatContext, pkt);
    if (ret != 0) {
        sm_log("av_interleaved_write_frame error, stream index: %d, %d\n", pkt->stream_index, ret);
        return ret;
    }
    return ret;
}

int push_packet(RtspPusher* pusher, AVPacket *pkt) {
    if (pkt->stream_index == pusher->inputVStream->index) {
        pkt->stream_index = pusher->videoStream->index;
        av_packet_rescale_ts(pkt, pusher->inputVStream->time_base, pusher->videoStream->time_base);
    } else {
        pkt->stream_index = pusher->audioStream->index;
        av_packet_rescale_ts(pkt, pusher->inputAStream->time_base, pusher->audioStream->time_base);
    }
    pkt->pos = -1;

    int ret = pusher->push(pkt);
    if (ret != 0) {
        if (ret == AVERROR(EPIPE)) {
            sm_log("Connection closed");
        }
        return ret;
    }
    return 0;
}

int push_thread(void *data) {
    auto *pusher = (RtspPusher*)data;
    pusher->start_time = av_gettime();

    AVDictionary *srcOptions = nullptr;
    if (isRtsp(pusher->src)) {
        av_opt_set(&srcOptions, "rtsp_transport", "tcp", 0);
        av_opt_set(&srcOptions, "stimeout", "3000000", 0);
        av_opt_set(&srcOptions, "buffer_size", "8192000", 0);
    }

    pusher->inputFormatContext = avformat_alloc_context();
    int ret = avformat_open_input(&pusher->inputFormatContext, pusher->src, nullptr, &srcOptions);
    if (ret < 0) {
        sm_log("open input error, %d\n", ret);
        return ret;
    }

    avformat_find_stream_info(pusher->inputFormatContext, nullptr);

    av_dump_format(pusher->inputFormatContext, 0, pusher->src, 0);

    int v_index = av_find_best_stream(pusher->inputFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    int a_index = av_find_best_stream(pusher->inputFormatContext, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);

    for (int i = 0; i < pusher->inputFormatContext->nb_streams; i++) {
        AVStream *stream = pusher->inputFormatContext->streams[i];
        sm_log("Stream %d: %d", i, stream->codecpar->codec_id);
    }

    pusher->inputVStream = pusher->inputFormatContext->streams[v_index];
    pusher->inputAStream = pusher->inputFormatContext->streams[a_index];

    AVRational frame_rate = av_guess_frame_rate(pusher->inputFormatContext, pusher->inputVStream, nullptr);
    double v_duration_in = (frame_rate.num && frame_rate.den ? av_q2d((AVRational){frame_rate.den, frame_rate.num}) : 0);

    const AVCodec *srcVCodec = avcodec_find_decoder(pusher->inputVStream->codecpar->codec_id);
    if (srcVCodec == nullptr) {
        sm_log("cannot find decoder\n");
        return -1;
    }
    const AVCodec *srcACodec = avcodec_find_decoder(pusher->inputAStream->codecpar->codec_id);
    if (srcACodec == nullptr) {
        sm_log("cannot find decoder\n");
        return -1;
    }

    pusher->inputVCodecContext = avcodec_alloc_context3(srcVCodec);
    avcodec_parameters_to_context(pusher->inputVCodecContext, pusher->inputVStream->codecpar);
    //pusher->inputVCodecContext->pkt_timebase = pusher->inputVStream->time_base;
    ret = avcodec_open2(pusher->inputVCodecContext, srcVCodec, nullptr);
    if (ret != 0) {
        sm_log("avcodec_open2 error, %d\n", ret);
        return ret;
    }
    sm_log("Video stream rate, size: %dx%d, num: %d, den: %d, duration: %f\n",
           pusher->inputVStream->codecpar->width, pusher->inputVStream->codecpar->height,
           frame_rate.num, frame_rate.den, v_duration_in);


    pusher->inputACodecContext = avcodec_alloc_context3(srcACodec);
    avcodec_parameters_to_context(pusher->inputACodecContext, pusher->inputAStream->codecpar);
    //pusher->inputACodecContext->pkt_timebase = pusher->inputAStream->time_base;
    ret = avcodec_open2(pusher->inputACodecContext, srcACodec, nullptr);
    if (ret != 0) {
        sm_log("avcodec_open2 error, %d\n", ret);
        return ret;
    }

    frame_rate = pusher->inputAStream->time_base;
    double a_duration_in = (frame_rate.num && frame_rate.den ? av_q2d(pusher->inputAStream->time_base) : 0);
    sm_log("Audio stream rate, num: %d, den: %d, duration: %f\n",
           frame_rate.num, frame_rate.den, a_duration_in);
    av_dump_format(pusher->inputFormatContext, 0, pusher->src, 0);


    ret = newVideoCodecContext(pusher, pusher->videoCodecPar);
    if (ret < 0) {
        sm_log("Create output video codec context failed");
        return -1;
    }

    int src_sample_rate = pusher->inputACodecContext->sample_rate;
    int src_nb_channels = pusher->inputACodecContext->ch_layout.nb_channels;
    if (src_sample_rate != pusher->audioCodecPar->sample_rate) {
        pusher->audioCodecPar->sample_rate = src_sample_rate;
    }
    if (src_nb_channels != pusher->audioCodecPar->nb_channels) {
        pusher->audioCodecPar->nb_channels = src_nb_channels;
    }
    pusher->audioCodecPar->codecId = srcACodec->id;
    pusher->audioCodecPar->sample_fmt = pusher->inputACodecContext->sample_fmt;

    ret = newAudioCodecContext(pusher, pusher->audioCodecPar);
    if (ret < 0) {
        sm_log("Create output audio codec context failed");
        return -1;
    }

    ret = avformat_alloc_output_context2(&pusher->outFormatContext, nullptr, "rtsp", pusher->url);
    if (ret < 0) {
        sm_log("avformat_alloc_output_context2 error, %d, %s\n", ret, av_errStr(ret));
        return ret;
    }
    av_opt_set(pusher->outFormatContext->priv_data, "rtsp_transport", "tcp", 0);
    av_opt_set(pusher->outFormatContext->priv_data, "stimeout", "3000000", 0);
    av_opt_set(pusher->outFormatContext->priv_data, "rtpflags", "latm", 0);
    pusher->outFormatContext->max_interleave_delta = 10000;

    pusher->add_video_stream(pusher->inputVStream);
    pusher->add_audio_stream();

    if (pusher->outFormatContext->oformat->flags & AVFMT_GLOBALHEADER) {
        pusher->videoCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        pusher->audioCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    //av_dump_format(pusher->outFormatContext, 0, pusher->url, 0);

    if (!(pusher->outFormatContext->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&pusher->outFormatContext->pb, pusher->url, AVIO_FLAG_WRITE);
        if (ret < 0) {
            sm_log("avio_open output pb, %d, %s\n", ret, av_errStr(ret));
            return ret;
        }
    }
    AVDictionary *options = nullptr;
    av_dict_set(&options, "rtsp_transport", "tcp", 0);
    av_dict_set(&options, "buffer_size", "655360", 0);
    av_dict_set(&options, "stimeout", "3000000", 0);
    av_dict_set(&options, "video_track_timescale", "25", 0);
    ret = avformat_write_header(pusher->outFormatContext, &options);
    if (ret != AVSTREAM_INIT_IN_WRITE_HEADER) {
        sm_log("avformat_write_header error, %d, %s\n", ret, av_errStr(ret));
        return ret;
    }
    sm_log("avformat_write_header success");

    AVSampleFormat out_sample_fmt = pusher->audioCodecContext->sample_fmt; // audioCodecPar->sample_fmt;
    AVChannelLayout out_ch_layout = pusher->audioCodecContext->ch_layout;
    // av_channel_layout_default(&out_ch_layout, pusher->audioCodecPar->nb_channels);
    int out_sample_rate = pusher->audioCodecContext->sample_rate; // pusher->audioCodecPar->sample_rate;

    AVChannelLayout in_ch_layout = pusher->inputAStream->codecpar->ch_layout;
    AVSampleFormat in_sample_fmt = pusher->inputACodecContext->sample_fmt;
    int in_sample_rage = pusher->inputAStream->codecpar->sample_rate;

    SwrContext *swrContext = swr_alloc();
    ret = swr_alloc_set_opts2(&swrContext, &out_ch_layout, out_sample_fmt, out_sample_rate,
                              &in_ch_layout, in_sample_fmt, in_sample_rage,
                              0, nullptr);
    if (ret != 0) {
        av_log(nullptr, AV_LOG_ERROR, "swr_alloc_set_opts2 error, %d\n", ret);
        return ret;
    }
    ret = swr_init(swrContext);
    if (ret != 0) {
        av_log(nullptr, AV_LOG_ERROR, "swr_init error, %d\n", ret);
        return ret;
    }
    pusher->swrContext = swrContext;

//    AVRational frameRate = av_guess_frame_rate(formatCtx, srcVideoStream, nullptr);
    sm_log("input time base: num, %d, den, %d, %d, %d, %d, %d\n",
           pusher->inputVStream->time_base.num, pusher->inputVStream->time_base.den,
           pusher->inputVCodecContext->time_base.num, pusher->inputVCodecContext->time_base.den,
           pusher->inputVCodecContext->pkt_timebase.num, pusher->inputVCodecContext->pkt_timebase.den
           );
    sm_log("output time base: num, %d, den, %d\n",
           pusher->videoStream->time_base.num, pusher->videoStream->time_base.den);
           //pusher->videoCodecContext->time_base.num, pusher->videoCodecContext->time_base.den,
           //pusher->videoCodecContext->pkt_timebase.num, pusher->videoCodecContext->pkt_timebase.den

    //av_dump_format(pusher->outFormatContext, 0, pusher->url, 0);

    AVPacket *pkt = av_packet_alloc();
    while (!pusher->quit) {
        ret = av_read_frame(pusher->inputFormatContext, pkt);
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
        if (!pkt->data) {
            av_packet_unref(pkt);
            continue;
        }

        if (pkt->stream_index == v_index) {
            av_usleep(v_duration_in * AV_TIME_BASE);
            if (is_target_codec(pusher->inputVStream)) {
                ret = push_packet(pusher, pkt);
                if (ret != 0) {
                    if (ret == AVERROR(EPIPE)) {
                        sm_log("Connection closed");
                        break;
                    }
                    sm_log("send packet error, %d\n", ret);
                }
                continue;
            }


            //sm_log("pts: %lld, duration: %lld, n_pts: %lld, times: %lld\n", pkt->pts, pkt->duration, pts, times);
            // todo 关键
            //if (pkt->pts == AV_NOPTS_VALUE && pkt->dts == AV_NOPTS_VALUE) {
                av_packet_rescale_ts(pkt, pusher->inputVStream->time_base, pusher->videoStream->time_base);
//                sm_log("Original: duration: %f, rescale: pts: %lld, dts: %lld, duration: %lld, pts_time: %lld, t: %lld, now: %lld\n",
//                       duration, pkt->pts, pkt->dts, pkt->duration, pts_time, t, now);
                pkt->pos = -1;
            //}

            // int t = opts * av_q2d(pusher->inputVStream->time_base);

            //pkt->pts = pkt->dts = av_rescale_q(av_gettime() - pusher->start_time, AV_TIME_BASE_Q, pusher->videoStream->time_base);


            pkt->stream_index = pusher->videoStream->index;

            ret = pusher->push(pkt);
            if (ret != 0) {
                if (ret == AVERROR(EPIPE)) {
                    sm_log("Connection closed");
                    break;
                }
            }
        }

        if (pkt->stream_index == a_index) {
            if (is_target_codec(pusher->inputAStream)) {
                ret = push_packet(pusher, pkt);
                if (ret != 0) {
                    if (ret == AVERROR(EPIPE)) {
                        sm_log("Connection closed");
                        break;
                    }
                }
                av_usleep(a_duration_in * AV_TIME_BASE);
                continue;
            }

            ret = convert_audio_packet(pusher, pkt);
            if (ret != 0) {
                if (ret == AVERROR(EPIPE)) {
                    sm_log("Connection closed");
                    break;
                }
            }
            av_usleep(a_duration_in * AV_TIME_BASE);
        }
        // av_usleep(pkt->duration);

        av_packet_unref(pkt);


//        avcodec_send_packet(srcCodecCtx, pkt);
//        AVFrame *frame = av_frame_alloc();
        /*while (avcodec_receive_frame(srcCodecCtx, frame) == 0) {
            ret = avcodec_send_frame(pusher->videoCodecContext, frame);
            if (ret != 0) {
                if (ret == AVERROR(EAGAIN)) {
                    sm_log("avcodec_send_frame error AVERROR(EAGAIN), %d\n", ret);
                    continue;
                }
                if (ret == AVERROR(EINVAL)) {
                    sm_log("avcodec_send_frame error AVERROR(EINVAL), %d\n", ret);
                    continue;
                }
                sm_log("avcodec_send_frame error, %d\n", ret);
                continue;
            }
            AVPacket *encoded_pkt = av_packet_alloc();
            while (true) {
                ret = avcodec_receive_packet(pusher->videoCodecContext, encoded_pkt);
                if (ret != 0) {
                    break;
                }

                pusher->push(encoded_pkt);
            }
            av_packet_free(&encoded_pkt);
        }*/
    }
    av_write_trailer(pusher->outFormatContext);
    return 0;
}

void RtspPusher::start() {
    SDL_Thread *thread = SDL_CreateThread(push_thread, "rtsp_push_thread", this);
    int result = 0;
    SDL_WaitThread(thread, &result);
    sm_log("push thread finished, %d\n", result);
}

void RtspPusher::destroy() {
    if (this->swrContext) {
        swr_free(&this->swrContext);
    }

    avformat_close_input(&this->inputFormatContext);
    if (this->outFormatContext) {
        avio_close(this->outFormatContext->pb);
    }
    avformat_free_context(this->inputFormatContext);
    avformat_free_context(this->outFormatContext);

    sm_log("Destroy rtsp pusher");
}

RtspPusher::~RtspPusher() {
    // this->destroy();
}


static FILE *fp = nullptr;

void global_log_callback(void *ptr, int level, const char *fmt, va_list vl) {
//     vfprintf(stdout, fmt, vl);

    if(nullptr == fp) {
        fp = fopen("sms.log","a+");
    }
    if (fp) {
        time_t raw;
        struct tm *ptminfo;
        time(&raw);
        ptminfo = localtime(&raw);

        char szTime[128] = { 0 };
        sprintf(szTime, "I:%4d-%02d-%02d %02d:%02d:%02d ",
                ptminfo->tm_year + 1900, ptminfo->tm_mon + 1, ptminfo->tm_mday,
                ptminfo->tm_hour, ptminfo->tm_min, ptminfo->tm_sec);
        fwrite(szTime, strlen(szTime), 1, fp);
        vfprintf(fp, fmt, vl);
        fflush(fp);
        // fclose(fp);

//        SDL_Log(fmt, vl);
    }
}

int main_test(int argv, char *args[]) {

    //av_log_set_callback(global_log_callback);

    const char* url = args[1];
    const char* target = nullptr;
    if (argv <= 2) {
        if (!url) {
            url = "https://appletree-mytime-samsungbrazil.amagi.tv/playlist.m3u8";
            //rtmp://media3.scctv.net/live/scctv_800";
            //"http://devimages.apple.com.edgekey.net/streaming/examples/bipbop_4x3/gear2/prog_index.m3u8";
            //"rtmp://ns8.indexforce.com/home/mystream";
            //rtsp://192.168.56.119:8554/stream1";
             url = "D:/Pitbull-Give-Me-Everything.mp4";
//            url = "rtsp://192.168.56.119:8554/live/test1";
            url = "rtsp://192.168.2.46:8554/test3";
//            url = "https://appletree-mytime-samsungbrazil.amagi.tv/playlist.m3u8";
            url = "http://play-live.ifeng.com/live/06OLEEWQKN4.m3u8";
//           url = "C:\\Users\\cyy\\Desktop\\1.mp4";
//           url = "rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mp4";
        }
        target = "rtsp://192.168.56.119:8554/live/stream";
//        target = "rtsp://192.168.2.46:8554/stream1";
    } else {
        target = args[2];
    }
    sm_log("Input: %s\n", url);

    sm_log("target %s\n", target);
    auto *rtspPusher = new RtspPusher(url, target);
    auto *codecPar = new VideoCodecPar;
    codecPar->codecId = AV_CODEC_ID_H264;
    codecPar->width = 1920;
    codecPar->height = 1080;

    auto *aCodePar = new AudioCodecPar;
    aCodePar->codecId = AV_CODEC_ID_AAC;
    aCodePar->sample_fmt = AV_SAMPLE_FMT_FLTP;
    aCodePar->sample_rate = 48000;
    aCodePar->nb_channels = 2;
    aCodePar->nb_samples = 1024;

    rtspPusher->init(codecPar, aCodePar);
    rtspPusher->start();
    rtspPusher->destroy();
    // delete(rtspPusher);

    if (fp != nullptr) {
        fclose(fp);
    }
    return 0;
}
