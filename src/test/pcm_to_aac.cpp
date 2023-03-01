//
// Created by cyy on 2023/2/21.
//

#include "iostream"

#ifdef __cplusplus
extern "C" {
#endif
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"
#include "libavutil/avutil.h"
#include "libavutil/audio_fifo.h"
#ifdef __cplusplus
}
#endif

typedef struct SwrOpts {
    AVChannelLayout out_ch_layout;
    AVSampleFormat out_sample_fmt;
    int out_sample_rate;

    AVChannelLayout in_ch_layout;
    AVSampleFormat in_sample_fmt;
    int in_sample_rate;
} SwrOpts;

int audio_frame_index = 0;

static int64_t frame_pts = 0;

static inline char* av_errStr(int errnum) {
    char* buffer = static_cast<char *>(malloc(AV_ERROR_MAX_STRING_SIZE));
    memset(buffer, 0, AV_ERROR_MAX_STRING_SIZE);
    return av_make_error_string(buffer, AV_ERROR_MAX_STRING_SIZE, errnum);
}

int init_swr_context(SwrContext **swrContext, SwrOpts *opts) {
    if (*swrContext) {
        swr_free(swrContext);
    }
    /*AVChannelLayout out_ch_layout = opts->out_ch_layout;
    AVSampleFormat out_sample_fmt = opts->out_sample_fmt;
    int out_sample_rate = opts->out_sample_rate;

    AVChannelLayout in_ch_layout = opts->in_ch_layout;
    AVSampleFormat in_sample_fmt = opts->in_sample_fmt;
    int in_sample_rate = opts->in_sample_rate;*/

    *swrContext = swr_alloc();
    swr_alloc_set_opts2(swrContext, &opts->out_ch_layout, opts->out_sample_fmt, opts->out_sample_rate,
                        &opts->in_ch_layout, opts->in_sample_fmt, opts->in_sample_rate, 0, nullptr);

    int ret = swr_init(*swrContext);
    if (ret < 0) {
        printf("swr_init failed. ret: %d\n", ret);
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

int resample(SwrContext *swrContext, AVCodecContext *codecContext, AVFrame *frame, uint8_t **dest_data, AVFrame *dest) {

//    int max_dst_nb_samples = av_rescale_rnd(frame->nb_samples, codecContext->sample_rate, frame->sample_rate, AV_ROUND_UP);
//
//        if (dst_nb_samples >= max_dst_nb_samples) {
//            max_dst_nb_samples = dst_nb_samples;
//            dest->nb_samples = dst_nb_samples;
//        }
//    av_frame_get_buffer(dest, 0);

    int64_t dst_nb_samples = frame->nb_samples;
    uint8_t **out = dest_data;
    if (!out) {
        dst_nb_samples = av_rescale_rnd(swr_get_delay(swrContext, frame->sample_rate) + frame->nb_samples,
                                        codecContext->sample_rate, frame->sample_rate, AV_ROUND_UP);
//        if (dst_nb_samples > max_dst_nb_samples) {
//            max_dst_nb_samples = dst_nb_samples;
//        }
        av_frame_get_buffer(dest, 0);
        out = dest->data;
    }

    int resample_count = swr_convert(swrContext, out, (int)dst_nb_samples, (const uint8_t **)(frame->extended_data), frame->nb_samples);
    if (resample_count <= 0) {
//        av_frame_free(&dest);
        printf("swr_convert failed. ret: %d\n", resample_count);
        return -1;
    }
    std::cout << "Resample count: " << resample_count << ", src: " << frame->nb_samples << ", dst: " << dst_nb_samples << std::endl;
    return resample_count;
}

int write_aac(AVFormatContext *oc, AVCodecContext *aCodecContext, AVAudioFifo *fifo, int stream_index, AVStream *inStream) {
    if (av_audio_fifo_size(fifo) < aCodecContext->frame_size) {
        return 0;
    }

    const int frame_size = FFMIN(av_audio_fifo_size(fifo), aCodecContext->frame_size);

    AVFrame *frame = av_frame_alloc();
    AVChannelLayout channelLayout;
    av_channel_layout_default(&channelLayout, aCodecContext->ch_layout.nb_channels);
    frame->sample_rate = aCodecContext->sample_rate;
    frame->ch_layout = channelLayout;
    frame->format = aCodecContext->sample_fmt;
    frame->nb_samples = frame_size;

    av_frame_get_buffer(frame, 0);

    int read_samples = av_audio_fifo_read(fifo, reinterpret_cast<void **>(frame->data), frame_size);
    std::cout << "Read " << read_samples << " samples from fifo" << std::endl;
    if (read_samples < frame_size) {
        av_frame_free(&frame);
        return -1;
    }

    frame->pts = frame_pts;
    frame_pts += frame->nb_samples;

    int ret = avcodec_send_frame(aCodecContext, frame);
    if (ret != 0) {
        av_frame_free(&frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            printf("avcodec_send_frame AVERROR_EOF. ret: %d\n", ret);
            return ret;
        } else {
            printf("avcodec_send_frame failed. ret: %d\n", ret);
            return ret;
        }
    }
    while (true) {
        AVPacket *destPkt = av_packet_alloc();
        ret = avcodec_receive_packet(aCodecContext, destPkt);
        if (ret != 0) {
            if (ret == AVERROR(EAGAIN)) {
                std::cout << "avcodec_receive_packet EAGAIN" << std::endl;
                break;
            } else if (ret == AVERROR_EOF) {
                printf("avcodec_receive_packet AVERROR_EOF. ret: %d\n", ret);
                break;
            }
            printf("avcodec_receive_packet failed. ret: %d\n", ret);
            break;
        }

        destPkt->stream_index = stream_index;
        printf("Encode packet: src: %d, pts: %lld, %lld, du: %lld, size: %d\n",
               frame->nb_samples, destPkt->pts, destPkt->dts, destPkt->duration, destPkt->size);

        ret = av_interleaved_write_frame(oc, destPkt);
        if (ret != 0) {
            printf("av_interleaved_write_frame failed. ret: %d\n", ret);
            break;
        }
//        av_packet_unref(destPkt);
        av_packet_free(&destPkt);
    }

    av_frame_unref(frame);
    return 0;
}

int write_pcm(FILE *file, AVCodecContext *codecContext, uint8_t **data, /* AVFrame *frame,*/ int nb_samples) {
    int sizePerSample = av_get_bytes_per_sample(codecContext->sample_fmt);
    for (int i = 0; i < nb_samples; i++) {
        for (int ch = 0; ch < codecContext->ch_layout.nb_channels; ch++) {
            if (!data[ch]) {
                continue;
            }
            fwrite(data[ch] + sizePerSample * i, sizePerSample, 1, file);
        }
    }
    return 0;
}

int main_() {
    const char* src = "rtsp://192.168.2.46:8554/test1";
//    src = "D:/Pitbull-Feel-This-Moment.mp4";
    const char* output_dir = "D:/ts/";
    const char* target = "test.aac";
    const char* target_pcm = "D:/ts/1.pcm";
    int output_target = 0;

    FILE *file = nullptr;
    if (output_target == 1) {
        file = fopen(target_pcm, "wb");
    }

    AVFormatContext *inputFormatContext = nullptr;
    int ret = avformat_open_input(&inputFormatContext, src, nullptr, nullptr);
    if (ret != 0) {
        printf("avformat_open_input failed. %d\n", ret);
        return -1;
    }

    avformat_find_stream_info(inputFormatContext, nullptr);

    int video_stream_index = av_find_best_stream(inputFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    int audio_stream_index = av_find_best_stream(inputFormatContext, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);

    av_dump_format(inputFormatContext, 0, src, 0);
    printf("Stream, video: #%d, audio: #%d\n", video_stream_index, audio_stream_index);

    // AVStream *ivStream = inputFormatContext->streams[video_stream_index];
    AVStream *iaStream = inputFormatContext->streams[audio_stream_index];

    const AVCodec *iaCodec = avcodec_find_decoder(iaStream->codecpar->codec_id);
    AVCodecContext *iaCodecContext = avcodec_alloc_context3(iaCodec);
    avcodec_parameters_to_context(iaCodecContext, iaStream->codecpar);

    ret = avcodec_open2(iaCodecContext, iaCodec, nullptr);
    if (ret < 0) {
        printf("avcodec_open2 failed. ret: %d\n", ret);
        return -1;
    }

    AVFormatContext *oc = nullptr;
    const AVOutputFormat *pOutputFormat = av_guess_format("aac", target, nullptr);
    ret = avformat_alloc_output_context2(&oc, pOutputFormat, nullptr, target);
    if (ret < 0) {
        printf("avformat_alloc_output_context2 failed. ret: %d\n", ret);
        return -1;
    }

    // const AVCodec *vCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
    const AVCodec *aCodec = avcodec_find_encoder(oc->oformat->audio_codec);
    AVCodecContext *aCodecContext = avcodec_alloc_context3(aCodec);
    aCodecContext->ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    aCodecContext->sample_fmt = aCodec->sample_fmts[0];
    aCodecContext->sample_rate = 44100;
    aCodecContext->bit_rate = 128000;
    aCodecContext->codec_type = AVMEDIA_TYPE_AUDIO;
    aCodecContext->time_base = { 1, 44100 };

    /*AVStream *vStream = avformat_new_stream(oc, vCodec);
    avcodec_parameters_copy(vStream->codecpar, ivStream->codecpar);
    vStream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    vStream->time_base = { 1, 25 };
    vStream->r_frame_rate = { 24000, 1001 };*/

    AVStream  *aStream = avformat_new_stream(oc, aCodecContext->codec);
    aStream->id = (int)oc->nb_streams - 1;
    aStream->index = (int)oc->nb_streams - 1;
    aStream->codecpar->codec_tag = 0;
    aStream->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
    aStream->codecpar->bit_rate = aCodecContext->bit_rate;
    aStream->time_base = aCodecContext->time_base;
    aStream->start_time = iaStream->start_time;

    if (oc->oformat->flags & AVFMT_GLOBALHEADER) {
        aCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if (output_target == 1) {
        aCodecContext->sample_fmt = AV_SAMPLE_FMT_S16P;
    } else {
        aCodecContext->sample_fmt = AV_SAMPLE_FMT_FLTP;

        ret = avcodec_open2(aCodecContext, aCodec, nullptr);
        if (ret < 0) {
            printf("avcodec_open2 encode codec context failed. ret: %d\n", ret);
            return -1;
        }
        avcodec_parameters_from_context(aStream->codecpar, aCodecContext);
    }

    AVChannelLayout out_ch_layout;
    av_channel_layout_copy(&out_ch_layout, &aCodecContext->ch_layout);
    AVSampleFormat out_sample_fmt = aCodecContext->sample_fmt;
    int out_sample_rate = aCodecContext->sample_rate;

    AVChannelLayout in_ch_layout;
    av_channel_layout_copy(&in_ch_layout, &iaStream->codecpar->ch_layout);
    AVSampleFormat in_sample_fmt = iaCodecContext->sample_fmt;
    int in_sample_rate = iaCodecContext->sample_rate;

    SwrOpts opts = {
            out_ch_layout, out_sample_fmt, out_sample_rate,
            in_ch_layout, in_sample_fmt, in_sample_rate,
    };

    SwrContext *swrContext = nullptr; // swr_alloc();
    ret = init_swr_context(&swrContext, &opts);
    if (ret != 0) {
        printf("init swr context failed. %d\n", ret);
        return -1;
    }

    AVAudioFifo *fifo = av_audio_fifo_alloc(aCodecContext->sample_fmt, aCodecContext->ch_layout.nb_channels, aCodecContext->frame_size);

    char url[255] = {0};
    strcat(url, output_dir);
    strcat(url, target);

    av_dump_format(oc, 0, url, 1);

    ret = avio_open(&oc->pb, url, AVIO_FLAG_WRITE);
    if (ret < 0) {
        printf("avio_open2 failed. ret: %d\n", ret);
        return -1;
    }

    ret = avformat_write_header(oc, nullptr);
    if (ret < 0) {
        printf("avformat_write_header failed. ret: %d, %s\n", ret, av_errStr(ret));
        return -1;
    }

    int a_frame_index = 0;
    AVPacket *destPkt = av_packet_alloc();
    int64_t dest_pts = 0;

    AVPacket *pkt = av_packet_alloc();
    while (av_read_frame(inputFormatContext, pkt) == 0) {
        if (pkt->stream_index != audio_stream_index) {
            continue;
        }
        ret = avcodec_send_packet(iaCodecContext, pkt);
        if (ret != 0) {
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                continue;
            }
            printf("avcodec_send_packet failed. ret: %d\n", ret);
            break;
        }
        while (true) {
            AVFrame *frame = av_frame_alloc();
            ret = avcodec_receive_frame(iaCodecContext, frame);
            if (ret != 0) {
                av_frame_free(&frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                }
                printf("avcodec_receive_frame failed. ret: %d\n", ret);
                break;
            }
            AVChannelLayout channelLayout;
            av_channel_layout_default(&channelLayout, aCodecContext->ch_layout.nb_channels);
            AVFrame *dest = av_frame_alloc();
            dest->sample_rate = aCodecContext->sample_rate;
            dest->ch_layout = channelLayout;
            dest->format = aCodecContext->sample_fmt;
            dest->nb_samples = aCodecContext->frame_size;

//            uint8_t *data[2] = {0};
//            data[0] = (uint8_t *)av_malloc(1152 * 8);
//            data[1] = (uint8_t *)av_malloc(1152 * 8);

            int count = resample(swrContext, aCodecContext, frame, nullptr, dest);
            dest_pts += count;
            dest->pts = dest_pts;
            dest->duration = 0;

            if ((ret = av_audio_fifo_realloc(fifo, av_audio_fifo_size(fifo) + count)) < 0) {
                fprintf(stderr, "Could not reallocate FIFO\n");
                return ret;
            }
            av_audio_fifo_write(fifo, reinterpret_cast<void **>(dest->data), count);

            if (output_target == 0) {
                // to aac
                ret = write_aac(oc, aCodecContext, fifo, aStream->index, iaStream);
                if (ret != 0) {
                    break;
                }
            }
            if (output_target == 1) {
//                write_pcm(file, aCodecContext, dest->data, count);
            }

            a_frame_index ++;
        }

        if (a_frame_index > 20000) {
            break;
        }
    }

//    av_free(data[0]);
//    av_free(data[1]);
//    data[0] = 0;
//    data[1] = 0;

    av_packet_free(&destPkt);

    if (output_target == 0) {
        av_write_trailer(oc);
        avio_close(oc->pb);
        avformat_free_context(oc);
    } else {
        fclose(file);
    }

    av_audio_fifo_free(fifo);

    avcodec_close(aCodecContext);
    avcodec_close(iaCodecContext);
    avformat_free_context(inputFormatContext);
    return 0;
}