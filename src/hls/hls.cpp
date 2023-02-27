//
// Created by cyy on 2023/1/3.
//

#include "hls.h"


HlsMuxer::HlsMuxer(const char* url, const char* outdir, const nlohmann::json* ext_args) {
    std::string source = url;
    auto id = Encrypt::md5(source);
    std::string dir = outdir;
    dir.append("/").append(id);
    this->outdir = dir;
    this->streamId = id;

    std::cout << "Output: " << dir << std::endl;

    this->extends_args = ext_args;

    this->playlist = new M3uPlaylist;
    this->playlist->url = url;
    this->playlist->outdir = this->outdir;
    this->playlist->target_duration = this->segment_duration;
    this->playlist->sequence = 0;

    this->exit = false;
}

AVFormatContext* HlsMuxer::new_output_context(const char* url, const std::vector<AVStream*>& streams) {
    const AVOutputFormat *pOutputFormat = av_guess_format("mpegts", url, nullptr);
    AVFormatContext *outputFmtContext = nullptr; // avformat_alloc_context();
    int ret = avformat_alloc_output_context2(&outputFmtContext, pOutputFormat, nullptr, url);
    if (ret != 0) {
        printf("avformat_alloc_output_context2 failed; ret: %d, %s", ret, av_errStr(ret));
    }
    outputFmtContext->debug = 1;

    for (auto stream : streams) {
        if (!stream) {
            break;
        }
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            auto stream_s = vc_new_stream(outputFmtContext, stream->codecpar->codec_id, stream);
            stream_s->time_base = { 1, 25 };
            stream_s->r_frame_rate = { 25, 1 };
            stream_s->avg_frame_rate = { 25, 1 };
            this->dst_video_stream_index = stream_s->index;
            this->videoContext->dstVideoStream = stream_s;
        } else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            AVStream *stream_s = nullptr;
            if (stream->codecpar->codec_id == this->videoContext->dstAudioCodecContext->codec_id) {
                stream_s = vc_new_stream(outputFmtContext, stream->codecpar->codec_id, stream);
            } else {
                stream_s = vc_new_stream(outputFmtContext, this->videoContext->dstAudioCodecContext->codec_id, nullptr);
                avcodec_parameters_from_context(stream_s->codecpar, this->videoContext->dstAudioCodecContext);
                stream_s->time_base = { 1, this->videoContext->dstAudioCodecContext->sample_rate };
            }
            this->dst_audio_stream_index = outputFmtContext->nb_streams - 1;
            this->videoContext->dstAudioStream = stream_s;
        }
    }

    ret = vc_open_writer(outputFmtContext, url);
    if (ret < 0) {
        printf("vc_open_writer error %d\n", ret);
        avformat_free_context(outputFmtContext);
        return nullptr;
    }
    return outputFmtContext;
}

[[maybe_unused]] int HlsMuxer::getStreamType(const std::string& url) {
    std::string path = toLower(url);
    bool stream = url.find("rtsp://") != std::string::npos || url.find("rtmp://") != std::string::npos;
    return stream ? 1 : 0;
}

std::string HlsMuxer::new_seg_file_name(int segment_index) {
    std::string name = HLS_SEG_FILE_PREFIX;
    name += std::to_string(segment_index) + ".ts";
    return name;
}

M3uSegment* HlsMuxer:: get_segment(int index) const {
    if (this->playlist->segments.empty() || this->playlist->segments.size() < index + 1) {
        return nullptr;
    }
    return this->playlist->segments[index];
}

int HlsMuxer::new_index_file() {
    int ret = create_dir(this->outdir.c_str(), false);
    if (ret < 0) {
        printf("create output dir error %d\n", ret);
        return ret;
    }

    std::string m3u_index_file;
    m3u_index_file.append(outdir).append("/").append(HLS_M3U_INDEX_FILE);
    FILE *file = fopen(m3u_index_file.c_str(), "w+");
    if (!file) {
        return -1;
    }
    this->playlist->file = file;
    return 0;
}

M3uSegment* HlsMuxer::new_seg_file(int seg_index, double_t duration) {
    std::string name = this->new_seg_file_name(seg_index);
    auto seg = new M3uSegment{
        .sequence = seg_index,
        .duration = duration,
    };
    strcpy(seg->seg_name, name.c_str());

    this->playlist->segments.push_back(seg);
    return seg;
}

std::string HlsMuxer::write_playlist_header(int sequence) const {
    std::stringstream ss;
    ss << "#EXTM3U\n#EXT-X-VERSION:3\n#EXT-X-ALLOW-CACHE:NO\n";
    ss << "#EXT-X-TARGETDURATION:" << this->playlist->target_duration << "\n";
    ss << "#EXT-X-MEDIA-SEQUENCE:" << sequence << "\n";
    std::string header = ss.str();
    return header;
}

std::string HlsMuxer::write_playlist_file_entry(const M3uSegment* seg) {
    std::string seg_info;
    if (seg == nullptr) {
        return seg_info;
    }
    if (seg->program_datetime > 0) {
        seg_info.append("#EXT-X-PROGRAM-DATE-TIME:");// + time_utc_str(ms) + "\n"
        seg_info.append(time_utc_str(seg->program_datetime));
        seg_info.append("\n");
    }
    seg_info.append("#EXTINF:" + std::to_string(seg->duration) + ",\n" + seg->seg_name + "\n");
    return seg_info;
}

int get_begin_sequence(const std::vector<M3uSegment*>& segments, int max = 0) {
    if (max <= 0) {
        return segments[0] ? segments[0]->sequence : 0;
    }
    size_t n = segments.size();
    if (n <= max) {
        return segments[0]->sequence;
    }
    size_t i = n - max;
    return segments[i]->sequence;
}

std::string HlsMuxer::write_playlist_file_entries(int start) {
    if (this->playlist->segments.empty()) {
        return "";
    }
    std::string content;

    size_t len = this->playlist->segments.size();
    for (auto i = start; i < len; i++) {
        auto item = this->playlist->segments[i];
        content.append(this->write_playlist_file_entry(item));
    }

    return content;
}

std::string HlsMuxer::write_playlist_file_end() {
    static std::string content = "#EXT-X-ENDLIST\n";
    return content;
}

/**
 * Write m3u index file
 * @param type 0, VOD; 1, EVENT
 * @return
 */
int HlsMuxer::write_playlist(int type) {
    int max_items = MAX_SEGMENTS;
    int begin = get_begin_sequence(this->playlist->segments, max_items);
    this->playlist->sequence = begin;

    std::string content;
    content.append(write_playlist_header(type == 0 ? 0 : begin));

    if (type == 0) {
        content.append(write_playlist_file_entries());
        content.append(this->write_playlist_file_end());
    } else {
        content.append(write_playlist_file_entries(begin));
    }

    int ret = this->new_index_file();
    if (ret < 0) {
        return ret;
    }
    fwrite(content.c_str(), content.length(), 1, this->playlist->file);
    fclose(this->playlist->file);
    this->playlist->file = nullptr;
    return 0;
}

void HlsMuxer::close() {
//    if (this->playlist->file) {
//        fclose(this->playlist->file);
//    }
    sm_log("Segment count: ", this->playlist->segments.size());
    this->playlist->segments.clear();
    this->exit = true;
}

void segment_file_clean_thread(const HlsMuxer* hlsMuxer) {
    auto playlist = hlsMuxer->playlist;
    while (!hlsMuxer->exit) {
        if (playlist && playlist->segments.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(3000));
            continue;
        }

        try {
            int last = playlist->segments[playlist->segments.size() - 1]->sequence;
            int i = last - MAX_SEGMENTS - 2;
            while (i >= 0) {
                std::string file = playlist->outdir + "/";
                file.append(HLS_SEG_FILE_PREFIX).append(std::to_string(i--) + ".ts");

                if (!std::filesystem::exists(file)) {
                    break;
                }
                std::filesystem::remove(file);
            }
        } catch (std::exception& e) {
            sm_log("Clear segment files error; ", e.what());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    }
}

void HlsMuxer::seg_clear() {
    try {
        std::thread th_clean(segment_file_clean_thread, this);
        th_clean.detach();
    } catch (std::exception& e) {
        sm_log("Start clean thread failed; ", e.what());
    }
}

int encode_audio_frame(AVCodecContext *codecContext, AVFrame *frame, AVPacket *encoded) {
    int ret = avcodec_send_frame(codecContext, frame);
    if (ret != 0) {
//        sm_log("avcodec_send_frame failed; ret: ");
        return ret;
    }
    while (true) {
        ret = avcodec_receive_packet(codecContext, encoded);
        if (ret != 0) {
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                return 0;
            }
            //sm_log("avcodec_receive_packet failed; ret: ", ret, ", ", av_errStr(ret));
            return ret;
        }
    }
    return 0;
}
/*

int HlsMuxer::write_convert_packet(AVFormatContext *outputFormatContext, AVPacket *pkt) {
    if (this->swrContext == nullptr) {
        SwrContextOpts swrOpts = {
                this->videoContext->dstAudioCodecContext->ch_layout,
                this->videoContext->dstAudioCodecContext->sample_fmt,
                this->videoContext->dstAudioCodecContext->sample_rate,

                this->videoContext->inAudioCodecContext->ch_layout,
                this->videoContext->inAudioCodecContext->sample_fmt,
                this->videoContext->inAudioCodecContext->sample_rate,
        };
        int ret = init_swr_context(&this->swrContext, &swrOpts);
        if (ret < 0) {
            return ret;
        }
        printf("Init swr context success\n");
    }

    avcodec_send_packet(this->videoContext->inAudioCodecContext, pkt);
    AVFrame *frame = av_frame_alloc();

    AVFrame *dest = av_frame_alloc();
    AVChannelLayout outLayout;
    av_channel_layout_default(&outLayout, this->videoContext->dstAudioCodecContext->ch_layout.nb_channels);
    dest->sample_rate = this->videoContext->dstAudioCodecContext->sample_rate;
    dest->ch_layout = outLayout;
    dest->format = this->videoContext->dstAudioCodecContext->sample_fmt;
    dest->nb_samples = 1024;
    int ret = av_frame_get_buffer(dest, 0);
    if (ret != 0) {
        printf("av_frame_get_buffer failed; ret: %d, %s\n", ret, av_errStr(ret));
        av_frame_free(&dest);
        return ret;
    }

    while (avcodec_receive_frame(this->videoContext->inAudioCodecContext, frame) >= 0) {
        int out_count = dest->nb_samples * dest->sample_rate / frame->sample_rate + 256;
        int ret_samples = swr_convert(this->swrContext, dest->data, out_count,
                                      (const uint8_t **)(frame->extended_data), frame->nb_samples);
        if (ret_samples < 0) {
            printf("Error while converting\n");
            continue;
        }
        AVPacket *dstPkt = av_packet_alloc();
        ret = encode_audio_frame(this->videoContext->dstAudioCodecContext, dest, dstPkt);
        if (ret != 0) {
            av_frame_free(&dest);
            av_packet_free(&dstPkt);
            continue;
        }
        dstPkt->pts = pkt->pts;
        dstPkt->dts = pkt->dts;
        av_packet_rescale_ts(dstPkt, videoContext->inAudioStream->time_base,
                             outputFormatContext->streams[this->dst_audio_stream_index]->time_base);
        printf("Write audio packet, pts: %lld\n", dstPkt->pts);
        ret = av_interleaved_write_frame(outputFormatContext, dstPkt);
        if (ret < 0) {
            printf("av_interleaved_write_frame error %d\n", ret);
        }
        av_frame_free(&dest);
        av_packet_free(&dstPkt);
    }
    return 0;
}
*/

int HlsMuxer::decode_audio_packet(AVPacket *pkt) {
    int ret = avcodec_send_packet(this->videoContext->inAudioCodecContext, pkt);
    if (ret != 0) {
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return 0;
        }
        return ret;
    }
    AVFrame *frame = av_frame_alloc();
    while (true) {
        ret = avcodec_receive_frame(this->videoContext->inAudioCodecContext, frame);
        if (ret != 0) {
            av_frame_free(&frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                return 0;
            }
            return ret;
        }

        AVFrame *dest = av_frame_alloc();
        dest->ch_layout = this->videoContext->dstAudioCodecContext->ch_layout;
        dest->format = this->videoContext->dstAudioCodecContext->sample_fmt;
        dest->sample_rate = this->videoContext->dstAudioCodecContext->sample_rate;
        dest->nb_samples = this->videoContext->dstAudioCodecContext->frame_size;

        av_frame_get_buffer(dest, 0);
        int resample_count = vc_swr_resample(this->swrContext, frame, dest);

        if ((ret = av_audio_fifo_realloc(this->videoContext->fifo, av_audio_fifo_size(this->videoContext->fifo) + resample_count)) < 0) {
            fprintf(stderr, "Could not reallocate FIFO\n");
            return ret;
        }
        av_audio_fifo_write(this->videoContext->fifo, reinterpret_cast<void **>(dest->data), resample_count);


    }
}

int HlsMuxer::write_audio_packet() {
    auto fifo = this->videoContext->fifo;
    auto dstCodecContext = this->videoContext->dstAudioCodecContext;

    if (av_audio_fifo_size(fifo) < dstCodecContext->frame_size) {
        return AVERROR(EAGAIN);
    }
    const int frame_size = FFMIN(av_audio_fifo_size(fifo), dstCodecContext->frame_size);

    AVFrame *frame = av_frame_alloc();
    AVChannelLayout channelLayout;
    av_channel_layout_default(&channelLayout, dstCodecContext->ch_layout.nb_channels);
    frame->sample_rate = dstCodecContext->sample_rate;
    frame->ch_layout = channelLayout;
    frame->format = dstCodecContext->sample_fmt;
    frame->nb_samples = frame_size;

    av_frame_get_buffer(frame, 0);

    int read_samples = av_audio_fifo_read(fifo, reinterpret_cast<void **>(frame->data), frame_size);
//    std::cout << "Read " << read_samples << " samples from fifo" << std::endl;
    if (read_samples < frame_size) {
        av_frame_free(&frame);
        return -1;
    }

    frame->pts = this->pts_a;
    this->pts_a += frame->nb_samples;

    int ret = avcodec_send_frame(dstCodecContext, frame);
    if (ret != 0) {
        av_frame_free(&frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            // printf("avcodec_send_frame AVERROR_EOF. ret: %d\n", ret);
            return 0;
        }
        printf("avcodec_send_frame failed. ret: %d\n", ret);
        return ret;
    }

    AVPacket *pkt = av_packet_alloc();
    while(true) {
        ret = avcodec_receive_packet(dstCodecContext, pkt);
        if (ret != 0) {
            av_packet_free(&pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                // printf("avcodec_receive_packet AVERROR_EOF. ret: %d\n", ret);
                break;
            } else {
                printf("avcodec_receive_packet failed. ret: %d\n", ret);
            }
            return ret;
        }

        pkt->stream_index = this->videoContext->dstAudioStream->index;
        ret = av_interleaved_write_frame(this->videoContext->dstFormatContext, pkt);
        if (ret != 0) {
            printf("av_interleaved_write_frame failed. ret: %d\n", ret);
            break;
        }
    }
    av_packet_free(&pkt);
    return 0;
}

int new_output_audioCodecContext(HlsMuxer *muxer) {
    const AVCodec *aCodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    auto aCodecContext = avcodec_alloc_context3(aCodec);
    aCodecContext->codec_type = AVMEDIA_TYPE_AUDIO;
    aCodecContext->codec_id = AV_CODEC_ID_AAC;
    aCodecContext->sample_fmt = AV_SAMPLE_FMT_FLTP;
    aCodecContext->ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    aCodecContext->sample_rate = 44100;
//    muxer->dstAudioCodecContext->profile = FF_PROFILE_AAC_LOW;
//    muxer->dstAudioCodecContext->bit_rate = 128000;
    muxer->videoContext->dstAudioCodecContext = aCodecContext;
    int ret = avcodec_open2(muxer->videoContext->dstAudioCodecContext, aCodec, nullptr);
//    if (ret != 0) {
//        printf("Cannot open output audio codec context; ret: %d, %s\n", ret, av_err2str(ret));
//        return ret;
//    }

    if (!muxer->videoContext->fifo) {
        muxer->videoContext->fifo = av_audio_fifo_alloc(aCodecContext->sample_fmt, aCodecContext->ch_layout.nb_channels, 1024);
    }
    return ret;
}

void HlsMuxer::set_packet_pts_dts(AVPacket *pkt, int frame_index) {
    if (pkt->pts == AV_NOPTS_VALUE) {
        if (pkt->dts != AV_NOPTS_VALUE) {
            pkt->pts = pkt->dts;
        } else {
            pkt->pts = pkt->dts = 0;
        }
    } else if (pkt->dts == AV_NOPTS_VALUE) {
        pkt->dts = pkt->pts;
    }
    if (pkt->pts < pkt->dts) {
        pkt->pts = pkt->dts;
    }
    pkt->pos = -1;

//    int64_t calc_duration = AV_TIME_BASE / av_q2d(this->videoContext->videoStream->r_frame_rate);
//    int64_t pts = (frame_index * calc_duration) / (av_q2d(this->videoContext->videoStream->time_base) * AV_TIME_BASE);
//    int64_t dts = pts;
//    int64_t duration = calc_duration / (av_q2d(this->videoContext->videoStream->time_base) * AV_TIME_BASE);
//
//    pkt->pts = pts;
//    pkt->dts = dts;
//    pkt->duration = duration;
}

int HlsMuxer::start() {
    /*int ret = this->new_index_file();
    if (ret < 0) {
        printf("create m3u index file error");
        return -1;
    }*/
    int ret = create_dir(this->outdir.c_str(), true);
    if (ret < 0) {
        printf("create output dir error %d\n", ret);
        return ret;
    }

    VideoContext *videoContext = vc_alloc_context();
    ret = vc_open_input(videoContext, this->playlist->url);
    if (ret != 0) {
        printf("cannot open media file, %d\n", ret);
        vc_free_context(videoContext);
        return -1;
    }

    av_dump_format(videoContext->inputFormatContext, 0, videoContext->file, 0);

    this->videoContext = videoContext;

    ret = new_output_audioCodecContext(this);
    if (ret != 0) {
        printf("Cannot open output audio codec context; ret: %d, %s\n", ret, av_errStr(ret));
        return -1;
    }

    //TODO clear thread
//    if (videoContext->duration < 0) {
//        this->seg_clear();
//    }

    //printf("Duration: %lld, %lld\n", videoContext->duration, videoContext->duration / AV_TIME_BASE);

    std::vector<AVStream*> streams = {
            videoContext->inVideoStream,
            videoContext->inAudioStream,
    };
    AVFormatContext *outputFmtContext = nullptr;

    int video_stream_index = videoContext->inVideoStream->index;
    int audio_stream_index = videoContext->inAudioStream->index;

    SwrContextOpts swrOpts = {
            this->videoContext->dstAudioCodecContext->ch_layout,
            this->videoContext->dstAudioCodecContext->sample_fmt,
            this->videoContext->dstAudioCodecContext->sample_rate,

            this->videoContext->inAudioCodecContext->ch_layout,
            this->videoContext->inAudioCodecContext->sample_fmt,
            this->videoContext->inAudioCodecContext->sample_rate,
    };
    ret = init_swr_context(&this->swrContext, &swrOpts);
    if (ret < 0) {
        return ret;
    }

    int64_t seg_index = -1;
    double_t seg_seconds = 0.0;
    double_t seg_duration = this->playlist->target_duration;
    bool set_seg_duration = false;
    int segment_index = 0;

    int frame_index = 0;
    AVPacket *pkt = av_packet_alloc();
    while(av_read_frame(videoContext->inputFormatContext, pkt) >= 0) {
        if (this->exit) {
            break;
        }

        if (pkt->stream_index == video_stream_index) {
        } else if (pkt->stream_index == audio_stream_index) {
            double_t time_sec = (double_t)pkt->pts * av_q2d(videoContext->inAudioStream->time_base);
            auto index = (int64_t)(time_sec / this->segment_duration);
            double_t duration = time_sec - seg_seconds;
            seg_duration = duration;
            //int64_t offset = time_sec % 5;

            if (seg_index == -1) {
                seg_index = index;
            }
            if (seg_index != index) {
                av_write_trailer(outputFmtContext);
                avio_close(outputFmtContext->pb);
                avformat_free_context(outputFmtContext);
                outputFmtContext = nullptr;

                M3uSegment* seg = this->playlist->segments[this->playlist->segments.size() - 1];
                seg->duration = duration;
                set_seg_duration = true;
                if (videoContext->duration < 0) {
                    this->write_playlist(1);

                    //TODO Send status
                    if (segment_index == 0) {
                        // this->send_status(1);
                    }
                }

                segment_index++;
                seg_index = index;
                seg_seconds = time_sec;
            }
        } else {
            continue;
        }

        //TODO New segment file
        if (outputFmtContext == nullptr) {
            printf("New segment %d\n", segment_index);
            M3uSegment *seg = this->get_segment(segment_index);
            if (!seg) {
                seg = this->new_seg_file(segment_index, this->playlist->target_duration);
                set_seg_duration = false;
            }
            std::string url = outdir;
            url.append("/").append(seg->seg_name);
            outputFmtContext = this->new_output_context(url.c_str(), streams);
            this->videoContext->dstFormatContext = outputFmtContext;
            if (segment_index == 0) {
                av_dump_format(outputFmtContext, 0, url.c_str(), 1);
            }
        }

        if (pkt->stream_index == video_stream_index) {
            double_t time_sec = pkt->pts * av_q2d(videoContext->inVideoStream->time_base);
//            printf("time: %f, pts: %lld, dts: %lld\n", time_sec * AV_TIME_BASE, pkt->pts, pkt->dts);

            this->set_packet_pts_dts(pkt, frame_index);
            av_packet_rescale_ts(pkt, videoContext->inVideoStream->time_base, outputFmtContext->streams[this->dst_video_stream_index]->time_base);

            ret = av_interleaved_write_frame(outputFmtContext, pkt);
            if (ret < 0) {
                printf("av_interleaved_write_frame error %d\n", ret);
            }
//            printf("Rescaled, time: %f, pts: %lld, dts: %lld\n", time_sec * AV_TIME_BASE, pkt->pts, pkt->dts);
            frame_index++;
        } else if (pkt->stream_index == audio_stream_index) {
            //int64_t time_sec = pkt->pts * av_q2d(videoContext->audioStream->time_base);
            //printf("time: %lld, pts: %lld\n", time_sec, pkt->pts);

            // this->videoContext->audioStream->codecpar->codec_id

//            if (this->videoContext->inVideoStream->codecpar->codec_id != this->videoContext->dstAudioCodecContext->codec_id) {
//                this->write_convert_packet(outputFmtContext, pkt);
//                continue;
//            }
//            av_packet_rescale_ts(pkt, videoContext->inAudioStream->time_base, outputFmtContext->streams[this->dst_audio_stream_index]->time_base);

            ret = this->decode_audio_packet(pkt);
            if (ret != 0) {
                av_log(nullptr, AV_LOG_ERROR, "Decode audio packet failed %d\n", ret);
                break;
            }
            ret = this->write_audio_packet();
            if (ret != 0) {
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    continue;
                }
                av_log(nullptr, AV_LOG_ERROR, "Write audio packet failed %d\n", ret);
            }
        } else {
            continue;
        }
    }

    //TODO Set last segment duration
    if (!set_seg_duration && !this->playlist->segments.empty()) {
        this->playlist->segments[this->playlist->segments.size() - 1]->duration = seg_duration;
    }

    if (videoContext->duration > 0) {
        this->write_playlist(0);
    }
    this->close();

    av_packet_free(&pkt);
    av_write_trailer(outputFmtContext);
    avformat_free_context(outputFmtContext);

    vc_free_context(videoContext);
    return 0;
}

void HlsMuxer::send_status(int action) {
    if (!check_extend_args(this->extends_args)) {
        return;
    }
    nlohmann::json data;
    data["action"] = action;
    if (action == 1) {
        data["id"] = this->streamId;
    }
    std::string msg;
    const httpCli::Response *res = nullptr;
    try {
        auto port = (*this->extends_args)["serverPort"].get<std::string>();
        auto url = (*this->extends_args)["url"].get<std::string>();
        auto server = "http://127.0.0.1:" + port;
        auto target = server + url;

        msg = "host: " + server + url + ", body: " + data.dump();
        res = httpCli::post(target.c_str(), data.dump().c_str(), "application/json");
        if (!res || res->statusCode != 200) {
            throw httpCli::RequestError("Send status failed");
        }
        std::cout << "Send status success, " << msg << std::endl;
    } catch (httpCli::RequestError& e) {
        std::cout << e.what();
        if (res) {
            std::cout << ", status code: " << res->statusCode << ", " << res->statusText << ", " << msg;
        }
        std::cout << std::endl;
    } catch (std::exception& e) {
        std::cout << "Send status error, " << e.what() << std::endl;
    }
}
