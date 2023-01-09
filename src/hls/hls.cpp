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
    this->playlist->target_duration = 5;
    this->playlist->sequence = 0;

    this->exit = false;
}

AVFormatContext* HlsMuxer::new_output_context(const AVOutputFormat *ofmt, const char* url, const std::vector<AVStream*>& streams) {
    AVFormatContext *outputFmtContext = avformat_alloc_context();
    outputFmtContext->oformat = ofmt;

    for (auto stream : streams) {
        if (!stream) {
            break;
        }
        vc_new_stream(outputFmtContext, stream->codecpar->codec_id, stream);
    }

    int ret = vc_open_writer(outputFmtContext, url);
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
    int ret = create_dir(this->outdir.c_str());
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
 * write m3u index file
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
    return 0;
}

void HlsMuxer::close() {
    if (this->playlist->file) {
        fclose(this->playlist->file);
    }
    printf("Segment count: %zu\n", this->playlist->segments.size());
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
            std::cout << "Clear segment files error; " << e.what() << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    }
}

void HlsMuxer::seg_clear() {
    try {
        std::thread th_clean(segment_file_clean_thread, this);
        th_clean.detach();
    } catch (std::exception& e) {
        std::cout << "Start clean thread failed; " << e.what() << std::endl;
    }
}

int HlsMuxer::start() {
    /*int ret = this->new_index_file();
    if (ret < 0) {
        printf("create m3u index file error");
        return -1;
    }*/
    int ret = create_dir(this->outdir.c_str());
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

    av_dump_format(videoContext->formatContext, 0, videoContext->file, 0);


    //TODO clear thread
    if (videoContext->duration < 0) {
        this->seg_clear();
    }

    //printf("Duration: %lld, %lld\n", videoContext->duration, videoContext->duration / AV_TIME_BASE);

    const AVOutputFormat *ofmt = av_guess_format("mpegts", nullptr, nullptr);
    std::vector<AVStream*> streams = {
            videoContext->videoStream,
            videoContext->audioStream,
    };
    AVFormatContext *outputFmtContext = nullptr;

    int video_stream_index = videoContext->videoStream->index;
    int audio_stream_index = videoContext->audioStream->index;

    int64_t seg_index = -1;
    double_t seg_seconds = 0.0;
    double_t seg_duration = this->playlist->target_duration;
    boolean set_seg_duration = false;
    int segment_index = 0;

    AVPacket *pkt = av_packet_alloc();
    while(av_read_frame(videoContext->formatContext, pkt) >= 0) {
        if (this->exit) {
            break;
        }

        if (pkt->stream_index == video_stream_index) {
        } else if (pkt->stream_index == audio_stream_index) {
            double_t time_sec = (double_t)pkt->pts * av_q2d(videoContext->audioStream->time_base);
            auto index = (int64_t)(time_sec / 5);
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
                        this->send_status(1);
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
            // printf("new segment %d\n", segment_index);
            M3uSegment *seg = this->get_segment(segment_index);
            if (!seg) {
                seg = this->new_seg_file(segment_index, this->playlist->target_duration);
                set_seg_duration = false;
            }
            std::string url = outdir;
            url.append("/").append(seg->seg_name);
            outputFmtContext = this->new_output_context(ofmt, url.c_str(), streams);
        }

        if (pkt->stream_index == video_stream_index) {
            //double_t time_sec = pkt->pts * av_q2d(videoContext->videoStream->time_base);
            //printf("time: %f, %lld, pts: %lld\n", time_sec * AV_TIME_BASE, time_microSec, pkt->pts);

            av_packet_rescale_ts(pkt, videoContext->videoStream->time_base, outputFmtContext->streams[0]->time_base);
        } else if (pkt->stream_index == audio_stream_index) {
            //int64_t time_sec = pkt->pts * av_q2d(videoContext->audioStream->time_base);
            //printf("time: %lld, pts: %lld\n", time_sec, pkt->pts);

            av_packet_rescale_ts(pkt, videoContext->audioStream->time_base, outputFmtContext->streams[1]->time_base);
        } else {
            continue;
        }

        ret = av_interleaved_write_frame(outputFmtContext, pkt);
        if (ret < 0) {
            printf("av_interleaved_write_frame error %d\n", ret);
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
