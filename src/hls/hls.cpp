//
// Created by cyy on 2023/1/3.
//

#include "hls.h"

//TODO Heart beat
void heartbeat(HlsMuxer* muxer) {
    while (!muxer->exit) {
        nlojson response = muxer->send_status(1);
        if (response.is_null() && muxer->retry >= HlsMuxer::HEARTBEAT_MAX_RETRY) {
            muxer->exit = true;
        }
        if (!response.is_null()) {
            auto expired = response.at("expired").get<int>();
            if (expired) {
                sm_warn("Hls session expired");
                muxer->exit = true;
            }
        }
        sleep(muxer->HEARTBEAT_INTERVAL);
    }
}

//Todo Segment clear thread
void segment_file_clean_thread(const HlsMuxer* hlsMuxer) {
    auto playlist = hlsMuxer->playlist;
    while (!hlsMuxer->exit) {
        if (!playlist || playlist->segments.empty() || playlist->segments.size() == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(3000));
            continue;
        }
        /*if (playlist->segments.size() > MAX_SEGMENTS) {
            sm_log("Segment count: {}", playlist->segments.size());
        }*/
        try {
            int last_seq = playlist->segments[playlist->segments.size() - 1]->sequence;
            int last = last_seq - MAX_SEGMENTS - 2;
            if (last <= 0) {
                continue;
            }

            while (last >= 0) {
                std::string file = playlist->outdir + "/";
                file.append(HLS_SEG_FILE_PREFIX).append(std::to_string(last) + ".ts");
                if (!std::filesystem::exists(file)) {
                    break;
                }
                std::filesystem::remove(file);
                last--;
            }
        } catch (std::exception& e) {
            sm_error("Clear segment files error; {}", e.what());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    }
}

HlsMuxer::HlsMuxer(const CmdArgs* args) {
    this->cmdArgs = args;

    std::string source = args->source;
    auto id = md5(source);
    std::string dir = args->dest;
    dir.append("/").append(id);
    this->outdir = dir;
    this->streamId = id;

    this->playlist = new M3uPlaylist;
    this->playlist->url = args->source.c_str();
    this->playlist->outdir = this->outdir;
    this->playlist->target_duration = this->segment_duration;
    this->playlist->sequence = 0;
    this->playlist->segments.reserve(MAX_SEGMENTS * 2);

    this->exit = false;
}

AVFormatContext* HlsMuxer::new_output_context(const char* url, const std::vector<AVStream*>& streams) {
    const AVOutputFormat *pOutputFormat = av_guess_format("mpegts", url, nullptr);
    AVFormatContext *outputFmtContext = nullptr; // avformat_alloc_context();
    int ret = avformat_alloc_output_context2(&outputFmtContext, pOutputFormat, nullptr, url);
    if (ret != 0) {
        sm_error("avformat_alloc_output_context2 failed; ret: {}, {}", ret, av_errStr(ret));
        return nullptr;
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
                // stream_s->time_base = { 1, this->videoContext->dstAudioCodecContext->sample_rate };
            }
            this->dst_audio_stream_index = outputFmtContext->nb_streams - 1;
            this->videoContext->dstAudioStream = stream_s;
        }
    }

    ret = vc_open_writer(outputFmtContext, url);
    if (ret < 0) {
        sm_error("vc_open_writer error {}", ret);
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
    if (this->playlist->segments.empty() || (int)this->playlist->segments.size() < index + 1) {
        return nullptr;
    }
    return this->playlist->segments[index];
}

int HlsMuxer::new_index_file() {
    int ret = create_dir(this->outdir.c_str(), false);
    if (ret < 0) {
        sm_error("Create output dir error {}", ret);
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
    int n = segments.size();
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

    for (auto iter = this->playlist->segments.begin(); iter != this->playlist->segments.end(); iter++) {
        auto seq = (*iter)->sequence;
        if (start <= seq) {
            content.append(this->write_playlist_file_entry(*iter));
        }
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
    int begin = get_begin_sequence(this->playlist->segments, MAX_SEGMENTS);
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
    //sm_log("Segment count: {}", this->playlist->segments.size());
    this->playlist->segments.clear();
    this->exit = true;

    vc_free_context(videoContext);
}

void HlsMuxer::heart_beat() {
    try {
        std::thread th_hb(heartbeat, this);
        th_hb.detach();
    } catch (std::exception& e) {
        sm_error("Start heart beat thread failed; {}", e.what());
        this->exit = true;
    }
}

void HlsMuxer::seg_clear() {
    try {
        std::thread th_clean(segment_file_clean_thread, this);
        th_clean.detach();
    } catch (std::exception& e) {
        sm_error("Start clean thread failed; {}", e.what());
    }
}

int HlsMuxer::decode_audio_packet(AVPacket *pkt) {
    int ret = avcodec_send_packet(this->videoContext->inAudioCodecContext, pkt);
    if (ret != 0) {
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return 0;
        }
        return ret;
    }
    AVFrame *frame = av_frame_alloc();

    AVFrame *dest = av_frame_alloc();
    dest->ch_layout = this->videoContext->dstAudioCodecContext->ch_layout;
    dest->format = this->videoContext->dstAudioCodecContext->sample_fmt;
    dest->sample_rate = this->videoContext->dstAudioCodecContext->sample_rate;
    dest->nb_samples = this->videoContext->dstAudioCodecContext->frame_size;

    ret = av_frame_get_buffer(dest, 0);
    if (ret != 0) {
        av_frame_free(&dest);
        av_frame_free(&frame);

        sm_error("av_frame_get_buffer failed. ret: {}, {}", ret, av_errStr(ret));
        return ret;
    }

    while (true) {
        ret = avcodec_receive_frame(this->videoContext->inAudioCodecContext, frame);
        if (ret != 0) {
            //av_frame_unref(frame);
            if (ret == AVERROR(EAGAIN)) { // || ret == AVERROR_EOF) {
                //return 0;
            }
            break;
        }

        int resample_count = vc_swr_resample(this->videoContext->swr, frame, dest);
        if ((ret = av_audio_fifo_realloc(this->videoContext->fifo, av_audio_fifo_size(this->videoContext->fifo) + resample_count)) < 0) {
            sm_error("Could not reallocate FIFO");
            break;
        }
        av_audio_fifo_write(this->videoContext->fifo, reinterpret_cast<void **>(dest->data), resample_count);

        av_frame_unref(dest);
        av_frame_unref(frame);
    }
    av_frame_free(&dest);
    av_frame_free(&frame);
    return ret;
}

int write_audio_packet(HlsMuxer *muxer, AVPacket *pkt) {
    av_packet_rescale_ts(pkt, muxer->videoContext->dstAudioCodecContext->time_base, muxer->videoContext->dstAudioStream->time_base);
    // sm_log("Rescale audio time, pts: {}, {}, {}", pkt->pts, pkt->dts, pkt->duration);

    pkt->stream_index = muxer->videoContext->dstAudioStream->index;
    return av_interleaved_write_frame(muxer->videoContext->dstFormatContext, pkt);
}

int HlsMuxer::write_encode_audio_packet() {
    auto fifo = this->videoContext->fifo;
    auto dstCodecContext = this->videoContext->dstAudioCodecContext;

    if (av_audio_fifo_size(fifo) < dstCodecContext->frame_size) {
        // sm_warn("No enough data to read");
        return AVERROR(EAGAIN);
    }
    // sm_log("FIFO size: {}", av_audio_fifo_size(fifo));
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
    if (read_samples < frame_size) {
        av_frame_free(&frame);
        sm_warn("No enough data to read, read samples: {}", read_samples);
        return -1;
    }

    int64_t pts = this->videoContext->audioClk->getStart() + frame->nb_samples;
    frame->pts = pts;
    frame->duration = frame_size;
    this->videoContext->audioClk->setStart(pts);

    int ret = avcodec_send_frame(dstCodecContext, frame);
    if (ret != 0) {
        av_frame_free(&frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            // printf("avcodec_send_frame AVERROR_EOF. ret: %d\n", ret);
            return 0;
        }
        sm_error("avcodec_send_frame failed. ret: {}", ret);
        return ret;
    }

    AVPacket *pkt = av_packet_alloc();
    while(true) {
        ret = avcodec_receive_packet(dstCodecContext, pkt);
        if (ret != 0) {
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                // printf("avcodec_receive_packet AVERROR_EOF. ret: %d\n", ret);
            } else {
                sm_error("avcodec_receive_packet failed. ret: {}", ret);
            }
            break;
        }

        ret = write_audio_packet(this, pkt);
        if (ret != 0) {
            sm_error("Write audio packet failed; av_interleaved_write_frame failed, ret: {}, {}", ret, av_errStr(ret));
            // break;
        }
    }
    av_frame_free(&frame);
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
    int ret = create_dir(this->outdir.c_str(), true);
    if (ret < 0) {
        sm_error("create output dir error {}", ret);
        return ret;
    }

    VideoContext *vc = vc_alloc_context();
    ret = vc_open_input(vc, this->playlist->url);
    if (ret != 0) {
        sm_error("cannot open media file, {}", ret);
        vc_free_context(vc);
        return -1;
    }
    sm_log("Open input success. {}", this->playlist->url);

    av_dump_format(vc->inputFormatContext, 0, vc->file, 0);

    this->videoContext = vc;

    ret = new_output_audioCodecContext(this);
    if (ret != 0) {
        sm_error("Cannot open output audio codec context; ret: {}, {}", ret, av_errStr(ret));
        return -1;
    }

    //TODO Start clear thread
    if (videoContext->duration < 0) {
        this->seg_clear();
    }

    //TODO Start heart beat thread
    if (!this->cmdArgs->standalone) {
        this->heart_beat();
    }
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
    ret = init_swr_context(&this->videoContext->swr, &swrOpts);
    if (ret < 0) {
        return ret;
    }

    //TODO Write index.m3u8, first
    ret = this->write_playlist(1);
    if (ret < 0) {
        sm_error("Write m3u8 index file failed");
        this->close();
        return -1;
    }

    int64_t seg_index = -1;
    double_t seg_seconds = 0.0;
    double_t seg_duration = this->playlist->target_duration;
    bool set_seg_duration = false;
    int segment_index = 0;

    int frame_index = 0;
    AVPacket *pkt = av_packet_alloc();
    while(!this->exit) {
        ret = av_read_frame(videoContext->inputFormatContext, pkt);
        if (ret < 0) {
            av_packet_unref(pkt);
            if (ret == AVERROR_EOF) {
                sm_log("av_read_frame eof");
                break;
            }
            sm_error("av_read_frame failed; ret: {}, {}", ret, av_errStr(ret));
            continue;
        }

        if (pkt->stream_index == video_stream_index) {
        } else if (pkt->stream_index == audio_stream_index) {
            double_t time_sec = (double_t)pkt->pts * av_q2d(videoContext->inAudioStream->time_base);
            auto index = (int64_t)(time_sec / this->segment_duration);
            double_t duration = time_sec - seg_seconds;
            if (duration <= 0) {
                av_packet_unref(pkt);
                continue;
            }
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
                    if (segment_index == 0 && !this->cmdArgs->standalone) {
                        this->send_status(1);
                    }
                }

                segment_index++;
                seg_index = index;
                seg_seconds = time_sec;
            }
        } else {
            av_packet_unref(pkt);
            continue;
        }

        //TODO New segment file
        if (outputFmtContext == nullptr) {
            sm_log("New segment {}", segment_index);
            M3uSegment *seg = this->get_segment(segment_index);
            if (!seg) {
                seg = this->new_seg_file(segment_index, this->playlist->target_duration);
                set_seg_duration = false;

                //TODO Erase segments list
                if (this->playlist->segments.size() > MAX_SEGMENTS) {
                    this->playlist->segments.erase(this->playlist->segments.begin());
                }
            }
            std::string url = outdir;
            url.append("/").append(seg->seg_name);
            outputFmtContext = this->new_output_context(url.c_str(), streams);
            this->videoContext->dstFormatContext = outputFmtContext;
                        this->videoContext->audioClk->setStart(0);

            if (segment_index == 0) {
                av_dump_format(outputFmtContext, 0, url.c_str(), 1);
            }

            /*std::cout << "OuputFormatContext opts:" << std::endl;
            std::cout << "    Video, time_base"
                      << ", src: " << av_rational_str(this->videoContext->inVideoStream->time_base)
                      << ", dst: " << av_rational_str(this->videoContext->dstVideoStream->time_base)
                      << std::endl;

            std::cout << "    Audio, time_base"
                      << ", src: " << av_rational_str(this->videoContext->inAudioStream->time_base)
                      << ", dst: " << av_rational_str(this->videoContext->dstAudioStream->time_base)
                      << std::endl;*/
        }

        if (pkt->stream_index == video_stream_index) {
            // double_t time_sec = pkt->pts * av_q2d(videoContext->inVideoStream->time_base);
            // sm_log("Video, time: {}, pts: {}, du: {}", time_sec, pkt->pts, pkt->duration);

            this->set_packet_pts_dts(pkt, frame_index);
            av_packet_rescale_ts(pkt, videoContext->inVideoStream->time_base, outputFmtContext->streams[this->dst_video_stream_index]->time_base);

//            sm_log("Rescale ts, pts: {}, du: {}, tb: {} {}, tb1: {} {}",
//                   pkt->pts, pkt->duration,
//                   videoContext->inVideoStream->time_base.den, videoContext->inVideoStream->time_base.num,
//                   outputFmtContext->streams[this->dst_video_stream_index]->time_base.den, outputFmtContext->streams[this->dst_video_stream_index]->time_base.num);
            ret = av_interleaved_write_frame(outputFmtContext, pkt);
            if (ret < 0) {
                sm_error("Write video packet failed; av_interleaved_write_frame error {}, {}", ret, av_errStr(ret));
            }
            av_packet_unref(pkt);
//            printf("Rescaled, time: %f, pts: %lld, dts: %lld\n", time_sec * AV_TIME_BASE, pkt->pts, pkt->dts);
            frame_index++;
        } else if (pkt->stream_index == audio_stream_index) {
            // double_t time_sec = pkt->pts * av_q2d(videoContext->inAudioStream->time_base);
            // sm_log("Audio time: {}, pts: {}, du: {}", time_sec, pkt->pts, pkt->duration);

            //todo same codec
            if (this->videoContext->inVideoStream->codecpar->codec_id == this->videoContext->dstAudioCodecContext->codec_id) {
                ret = write_audio_packet(this, pkt);
                if (ret != 0) {
                    sm_error("Write audio packet failed; av_interleaved_write_frame failed, ret: {}, {}", ret, av_errStr(ret));
                    // break;
                }
                av_packet_unref(pkt);
                continue;
            }
//            av_packet_rescale_ts(pkt, videoContext->inAudioStream->time_base, outputFmtContext->streams[this->dst_audio_stream_index]->time_base);
            if (this->videoContext->audioClk->getStart() == 0) {
                this->videoContext->audioClk->setStart(pkt->pts);
            }

            ret = this->decode_audio_packet(pkt);
            if (ret != 0) {
                av_packet_unref(pkt);
                if (ret != AVERROR(EAGAIN)) {
                    sm_error("Decode audio packet failed {}", ret);
                    continue;
                }
            }
            ret = this->write_encode_audio_packet();
            if (ret != 0) {
                av_packet_unref(pkt);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    continue;
                }
                sm_error("Write audio packet failed {}", ret);
            }
            av_packet_unref(pkt);
        } else {
            av_packet_unref(pkt);
            continue;
        }
    }

    sm_log("Hls muxer exited");

    //TODO Set last segment duration
    if (!set_seg_duration && !this->playlist->segments.empty()) {
        this->playlist->segments[this->playlist->segments.size() - 1]->duration = seg_duration;
    }

    if (videoContext->duration > 0) {
        this->write_playlist(0);
    }

    av_packet_free(&pkt);
    av_write_trailer(outputFmtContext);
    //avformat_free_context(outputFmtContext);

    this->close();
    return 0;
}

nlojson HlsMuxer::send_status(int action) {
    nlojson res;
    try {
        res = this->statusManager->send(action, this->streamId, this->cmdArgs->extraArgs);
        /*auto expired = res.at("expired").get<int>();
        if (expired) {
            sm_warn("Hls session expired");
            // this->exit = true;
        }*/
        //sm_log("{}", res.dump());
        this->retry = 0;
        return res;
    } catch (std::string& e) {
        sm_error("Send status error: {}", e);
    } catch (std::exception& e) {
        sm_error("Send status error; {}", e.what());
    } catch (...) {
        sm_error("Send status error;");
    }
    this->retry++;
    return res;
}
