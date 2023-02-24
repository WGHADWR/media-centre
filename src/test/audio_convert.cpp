//
// Created by cyy on 2022/11/29.
//

extern "C"
{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"
#include "libavutil/avutil.h"
#include "libavutil/time.h"

#include "SDL2/SDL.h"
}

#include <iostream>

using namespace std;

static int select_sample_rate(const AVCodec *codec)
{
    const int *p;
    int best_samplerate = 0;

    if (!codec->supported_samplerates)
        return 44100;

    p = codec->supported_samplerates;
    while (*p) {
        if (!best_samplerate || abs(44100 - *p) < abs(44100 - best_samplerate))
            best_samplerate = *p;
        p++;
    }
    return best_samplerate;
}

int main2(int argc, char *argv[]) {
    char* src = "D:/1.pcm";
    char* target = "D:/test.aac";

    int in_sample_rate = 8000;
    int in_nb_channels = 1;
    AVChannelLayout in_ch_layout;
    av_channel_layout_default(&in_ch_layout, 1);
    AVSampleFormat in_sample_fmt = AV_SAMPLE_FMT_S16;

    const AVCodec *aCodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    int sample_rate = select_sample_rate(aCodec);
    cout << "sample_rate: " << sample_rate << endl;

    AVChannelLayout outALayout;
    av_channel_layout_default(&outALayout, 2);
    AVCodecContext *outputACodecCtx = avcodec_alloc_context3(aCodec);
    outputACodecCtx->codec_type = AVMEDIA_TYPE_AUDIO;
    outputACodecCtx->sample_fmt = AV_SAMPLE_FMT_FLTP;
    outputACodecCtx->sample_rate = 8000;
    outputACodecCtx->ch_layout = outALayout;
    // outputACodecCtx->bit_rate = 64000;
    outputACodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    // outputACodecCtx->time_base = AVRational{ 1, outputACodecCtx->sample_rate };

    int ret = avcodec_open2(outputACodecCtx, aCodec, nullptr);
    if (ret != 0) {
        cout << "avcodec_open2 error " << ret << endl;
        return -1;
    }

    AVFormatContext *outputFormatCtx = nullptr;
    avformat_alloc_output_context2(&outputFormatCtx, nullptr, nullptr, target);

    AVStream *outAStream = avformat_new_stream(outputFormatCtx, nullptr);
    outAStream->codecpar->codec_tag = 0;
    avcodec_parameters_from_context(outAStream->codecpar, outputACodecCtx);
//    outAStream->time_base = outputACodecCtx->time_base;
//    outAStream->index = (int)outputFormatCtx->nb_streams - 1;

    // av_dump_format(outputFormatCtx, 0, target, 0);

    avio_open(&outputFormatCtx->pb, target, AVIO_FLAG_WRITE);
    ret = avformat_write_header(outputFormatCtx, nullptr);
    if (ret != 0) {
        cout << "avformat_write_header error " << ret << endl;
        return -1;
    }

    AVSampleFormat out_sample_fmt = outputACodecCtx->sample_fmt; // audioCodecPar->sample_fmt;
    AVChannelLayout out_ch_layout = outputACodecCtx->ch_layout;
    // av_channel_layout_default(&out_ch_layout, pusher->audioCodecPar->nb_channels);
    int out_sample_rate = outputACodecCtx->sample_rate; // pusher->audioCodecPar->sample_rate;

    SwrContext *swrContext = swr_alloc();
    ret = swr_alloc_set_opts2(&swrContext, &out_ch_layout, out_sample_fmt, out_sample_rate,
                              &in_ch_layout, in_sample_fmt, in_sample_rate,
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

    FILE *fp = fopen(src, "rb+");
    if (!fp) {
        av_log(nullptr, AV_LOG_ERROR, "cannot open src file");
        return -1;
    }

    AVFrame *frame = av_frame_alloc();
    frame->nb_samples = 1024;
    frame->sample_rate = out_sample_rate;
    frame->ch_layout = out_ch_layout;
    frame->format = AV_SAMPLE_FMT_FLTP;

    av_frame_get_buffer(frame, 0);

    char *buf = nullptr;
    int buf_size = 1024 * 4 * in_ch_layout.nb_channels;
//    int buf_size = av_samples_get_buffer_size(
//            nullptr, out_ch_layout.nb_channels, outputACodecCtx->frame_size, outputACodecCtx->sample_fmt, 1);
    buf = (char *)av_malloc(buf_size);


    AVPacket *pkt = av_packet_alloc();
    while (true) {
        int len = fread(buf, 1, buf_size, fp);
        if (len <= 0) {
            break;
        }
        const uint8_t *data[1];
        data[0] = (uint8_t *)buf;
        int out_sample_count = swr_convert(swrContext, frame->data, frame->nb_samples, data, frame->nb_samples);
        if (out_sample_count <= 0) {
            continue;
        }

        avcodec_send_frame(outputACodecCtx, frame);

        ret = avcodec_receive_packet(outputACodecCtx, pkt);
        if (ret != 0) {
            av_log(nullptr, AV_LOG_ERROR, "avcodec_receive_packet error, %d\n", ret);
            continue;
        }
        pkt->pts = pkt->dts = 0;
        ret = av_interleaved_write_frame(outputFormatCtx, pkt);
        if (ret != 0) {
            av_log(nullptr, AV_LOG_ERROR, "av_interleaved_write_frame error, %d\n", ret);
            continue;
        }
    }

    av_write_trailer(outputFormatCtx);

    avio_close(outputFormatCtx->pb);
    avcodec_close(outputACodecCtx);
    fclose(fp);
    return 0;
}

int main1(int argc, char *argv[]) {
    char* src = "D:/1.pcm";
    char* target = "D:/test.aac";

    AVFormatContext *inputFormatContext = avformat_alloc_context();

    int ret = avformat_open_input(&inputFormatContext, src, nullptr, nullptr);
    if (ret != 0) {
        cout << "Cannot open " << src << endl;
        return -1;
    }

    avformat_find_stream_info(inputFormatContext, nullptr);

    av_dump_format(inputFormatContext, 0, src, 0);

    int a_index = av_find_best_stream(inputFormatContext, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);

    cout << "Find audio stream " << a_index << endl;
    AVStream *aStream = inputFormatContext->streams[a_index];
    const AVCodec *srcACodec = avcodec_find_decoder(aStream->codecpar->codec_id);
    AVCodecContext *inputACodecCtx = avcodec_alloc_context3(srcACodec);
    avcodec_parameters_to_context(inputACodecCtx, aStream->codecpar);

    ret = avcodec_open2(inputACodecCtx, srcACodec, nullptr);
    if (ret != 0) {
        cout << "avcodec_open2 error inputACodecCtx " << ret << endl;
        return -1;
    }

    int sample_rate = select_sample_rate(srcACodec);
    cout << "sample_rate: " << sample_rate << endl;

    // Output
    const AVCodec *aCodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    sample_rate = select_sample_rate(aCodec);
    cout << "sample_rate: " << sample_rate << endl;

    AVChannelLayout outALayout;
    av_channel_layout_default(&outALayout, 2);
    AVCodecContext *outputACodecCtx = avcodec_alloc_context3(aCodec);
    outputACodecCtx->codec_type = AVMEDIA_TYPE_AUDIO;
    outputACodecCtx->sample_fmt = AV_SAMPLE_FMT_FLTP;
    outputACodecCtx->sample_rate = 8000;
    outputACodecCtx->ch_layout = outALayout;
    // outputACodecCtx->bit_rate = 64000;
    outputACodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    // outputACodecCtx->time_base = AVRational{ 1, outputACodecCtx->sample_rate };

    ret = avcodec_open2(outputACodecCtx, aCodec, nullptr);
    if (ret != 0) {
        cout << "avcodec_open2 error " << ret << endl;
        return -1;
    }

    AVFormatContext *outputFormatCtx = nullptr;
    avformat_alloc_output_context2(&outputFormatCtx, nullptr, nullptr, target);

    AVStream *outAStream = avformat_new_stream(outputFormatCtx, nullptr);
    outAStream->codecpar->codec_tag = 0;
    avcodec_parameters_from_context(outAStream->codecpar, outputACodecCtx);
//    outAStream->time_base = outputACodecCtx->time_base;
//    outAStream->index = (int)outputFormatCtx->nb_streams - 1;

    // av_dump_format(outputFormatCtx, 0, target, 0);

    avio_open(&outputFormatCtx->pb, target, AVIO_FLAG_WRITE);
    ret = avformat_write_header(outputFormatCtx, nullptr);
    if (ret != 0) {
        cout << "avformat_write_header error " << ret << endl;
        return -1;
    }

    AVSampleFormat out_sample_fmt = outputACodecCtx->sample_fmt; // audioCodecPar->sample_fmt;
    AVChannelLayout out_ch_layout = outputACodecCtx->ch_layout;
    // av_channel_layout_default(&out_ch_layout, pusher->audioCodecPar->nb_channels);
    int out_sample_rate = outputACodecCtx->sample_rate; // pusher->audioCodecPar->sample_rate;

    AVChannelLayout in_ch_layout = aStream->codecpar->ch_layout;
    AVSampleFormat in_sample_fmt = inputACodecCtx->sample_fmt;
    int in_sample_rage = aStream->codecpar->sample_rate;

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

//    FILE* fp = fopen(target, "wb");
//    if (!fp) {
//        cout << "open target file error" << endl;
//        return -1;
//    }

    int i = 0;
    AVPacket *pkt = av_packet_alloc();
    while (true) {
        ret = av_read_frame(inputFormatContext, pkt);
        if (ret < 0) {
            cout << "av_read_frame error " << ret << endl;
            if (ret == AVERROR_EOF) {
                break;
            }
            continue;
        }

        if (pkt->stream_index != a_index) {
            continue;
        }

        ret = avcodec_send_packet(inputACodecCtx, pkt);
        if (ret < 0) {
            cout << "avcodec_send_packet error, " << ret << endl;
            continue;
        }
        AVFrame *frame = av_frame_alloc();
        while (true) {
            ret = avcodec_receive_frame(inputACodecCtx, frame);
            if (ret < 0) {
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                }
                cout << "avcodec_receive_frame error, " << ret << endl;
            }

            cout << "nb_samples: " << frame->nb_samples << ", sample_rate: " << frame->sample_rate
                << ", pts: " << frame->pts << ", duration: " << frame->duration
                << ", size: " << strlen(reinterpret_cast<const char *>(frame->data[0]))
                << endl;

            // swr

            AVFrame *dstFrame = av_frame_alloc();
            AVChannelLayout channelLayout;
            av_channel_layout_default(&channelLayout, outputACodecCtx->ch_layout.nb_channels);
            // av_channel_layout_copy(&frame->ch_layout, &pusher->audioCodecContext->ch_layout);
            dstFrame->ch_layout = channelLayout;
            dstFrame->format = outputACodecCtx->sample_fmt;
            dstFrame->sample_rate = outputACodecCtx->sample_rate;
            dstFrame->nb_samples = outputACodecCtx->frame_size;
            av_frame_get_buffer(dstFrame, 0);

            ret = swr_convert(swrContext, dstFrame->data, dstFrame->nb_samples,
                              reinterpret_cast<const uint8_t **>(&frame->data), frame->nb_samples);

            cout << "swr " << ret << endl;
//            dstFrame->pts = 0;
//            dstFrame->pkt_dts = 0;
//            dstFrame->duration = 0;
            // end swr

            ret = avcodec_send_frame(outputACodecCtx, dstFrame);
            if (ret < 0) {
                cout << "avcodec_send_frame error " << ret << endl;
                continue;
            }
            AVPacket *dstPkt = av_packet_alloc();
            ret = avcodec_receive_packet(outputACodecCtx, dstPkt);
            if (ret < 0) {
                cout << "avcodec_receive_packet error " << ret << endl;
                continue;
            }
            dstPkt->stream_index = outAStream->index;
            dstPkt->pts = 0;
            dstPkt->dts = 0;

            ret = av_interleaved_write_frame(outputFormatCtx, dstPkt);
            if (ret < 0) {
                cout << "av_write_frame error " << ret << endl;
                continue;
            }
            av_packet_unref(dstPkt);
            av_frame_free(&dstFrame);
            // fwrite(dstPkt->data, 1, dstPkt->size, fp);
            /*ret = av_write_frame(outputFormatCtx, dstPkt);
            if (ret < 0) {
                cout << "av_write_frame error " << ret << endl;
                continue;
            }*/
            i++;
        }

        av_frame_free(&frame);
        av_packet_unref(pkt);
    }

    av_write_trailer(outputFormatCtx);

    //fclose(fp);

    avio_close(outputFormatCtx->pb);

    avcodec_close(outputACodecCtx);

    avformat_close_input(&inputFormatContext);
    avformat_free_context(inputFormatContext);
    return 0;
}