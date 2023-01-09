//
// Created by cyy on 2023/1/3.
//

#include "hls.h"

bool check_extend_args(const nlohmann::json* j) {
    if (!j) {
        return false;
    }
    if (!j->contains("serverPort") || !j->contains("url")) {
        return false;
    }
    try {
        auto port = (*j)["serverPort"].get<std::string>();
        // auto url = (*j)["url"].get<std::string>();
        std::stoi(port);
    } catch (std::exception& e) {
        return false;
    }
    return true;
}

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
    this->playlist->target_duration = 5;
    this->playlist->sequence = 0;

    this->exit = false;
}

AVFormatContext* HlsMuxer::new_output_context(const AVOutputFormat *ofmt, const char* url, const std::vector<AVStream*> streams) {
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

std::string HlsMuxer::new_seg_file_name(int segment_index) {
    std::string name = HLS_SEG_FILE_PREFIX;
    name += std::to_string(segment_index) + ".ts";
    return name;
}

M3uSegment* HlsMuxer:: get_segment(int index) {
    if (this->playlist->segments.empty() || this->playlist->segments.size() < index + 1) {
        return nullptr;
    }
    return &this->playlist->segments[index];
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

    this->playlist->segments.push_back(*seg);
    return seg;
}

int HlsMuxer::write_playlist_header() {
    std::stringstream ss;
    ss << "#EXTM3U\n#EXT-X-VERSION:3\n#EXT-X-ALLOW-CACHE:NO\n";
    ss << "#EXT-X-TARGETDURATION:" << this->playlist->target_duration << "\n";
    ss << "#EXT-X-MEDIA-SEQUENCE:" << this->playlist->sequence << "\n";
    std::string seg_data = ss.str();

    const char* data = seg_data.c_str();
    return fwrite(data, strlen(data), 1, this->playlist->file);
}

int HlsMuxer::write_playlist_file_entry(M3uSegment *seg) {
    //auto now = std::chrono::system_clock::now();
    //auto ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now).time_since_epoch().count();
    std::string seg_info;
    if (seg->program_datetime > 0) {
        seg_info.append("#EXT-X-PROGRAM-DATE-TIME:");// + time_utc_str(ms) + "\n"
        seg_info.append(time_utc_str(seg->program_datetime));
        seg_info.append("\n");
    }
    seg_info.append("#EXTINF:" + std::to_string(seg->duration) + ",\n" + seg->seg_name + "\n");

    const char* seg_item = seg_info.c_str();
    return fwrite(seg_item, strlen(seg_item), 1, this->playlist->file);
}

int HlsMuxer::write_playlist_file_end() {
    auto end = "#EXT-X-ENDLIST\n";
    return fwrite(end, strlen(end), 1, this->playlist->file);
}

void HlsMuxer::close() {
    if (this->playlist->file) {
        fclose(this->playlist->file);
    }
    printf("Segment count: %zu\n", this->playlist->segments.size());
    this->playlist->segments.clear();
}

int HlsMuxer::start() {
    int ret = this->new_index_file();
    if (ret < 0) {
        printf("create m3u index file error");
        return -1;
    }

    VideoContext *videoContext = vc_alloc_context();
    ret = vc_open(videoContext, this->playlist->url);
    if (ret != 0) {
        printf("cannot open media file, %d\n", ret);
        return -1;
    }

    av_dump_format(videoContext->formatContext, 0, videoContext->file, 0);

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
    int segment_index = 0;

    //TODO write m3u8 index file header
    this->write_playlist_header();

    AVPacket *pkt = av_packet_alloc();
    while(av_read_frame(videoContext->formatContext, pkt) >= 0) {
        if (this->exit) {
            break;
        }

        if (pkt->stream_index == video_stream_index) {
        } else if (pkt->stream_index == audio_stream_index) {
            double_t time_sec = pkt->pts * av_q2d(videoContext->audioStream->time_base);
            auto index = (int64_t)(time_sec / 5);
            //int64_t offset = time_sec % 5;

            if (seg_index == -1) {
                seg_index = index;
            }
            if (seg_index != index) {
                av_write_trailer(outputFmtContext);
                avformat_free_context(outputFmtContext);
                outputFmtContext = nullptr;

                double_t duration = time_sec - seg_seconds;

                M3uSegment seg = this->playlist->segments[this->playlist->segments.size() - 1];
                seg.duration = duration;
                this->write_playlist_file_entry(&seg);

                //TODO Send status
                if (segment_index == 0) {
                    this->send_status(1);
                }

                segment_index++;
                seg_index = index;
                seg_seconds = time_sec;
            }
        } else {
            continue;
        }

        if (!outputFmtContext) {
            // printf("new segment %d\n", segment_index);
            M3uSegment *seg = this->get_segment(segment_index);
            if (!seg) {
                seg = this->new_seg_file(segment_index, this->playlist->target_duration);
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

    this->write_playlist_file_end();
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
