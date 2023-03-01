//
// Created by cyy on 2023/1/3.
//

#ifndef VIDEOPLAYER_HLS_H
#define VIDEOPLAYER_HLS_H

#define __STDC_CONSTANT_MACROS

#include <vector>
#include <map>
#include <string>
#include <thread>
#include <chrono>

#include "../util/util.h"
#include <any>

#include "../http/httpcli.h"
#include "../av_utils/av_utils.h"
#include "../av_utils/video_context.h"

#define MAX_SEGMENTS 10

typedef struct M3uSegment {
    int32_t sequence;
    double_t duration;
    char seg_name[20];
    int64_t program_datetime;
} M3uSegment;

typedef struct M3uPlaylist {
    const char* url;
    int32_t target_duration;
    int32_t sequence;
    std::vector<M3uSegment*> segments;

    bool completed = false;
    std::string outdir;
    FILE *file;
} M3uPlaylist;

inline const char* HLS_M3U_INDEX_FILE = "index.m3u8";
inline const char* HLS_SEG_FILE_PREFIX = "seg_";

static bool check_extend_args(const std::map<std::string, std::any> * j) {
    if (!j) {
        return false;
    }

    if (j->find("serverPort")  == j->end() || j->find("url")  == j->end()) {
        return false;
    }
    try {
        auto port =   j->at("serverPort");
        // auto url = (*j)["url"].get<std::string>();
        std::any_cast<int>(port);
    } catch (std::exception& e) {
        return false;
    }
    return true;
}

class HlsMuxer {
public:
    int segment_duration = 10;
    M3uPlaylist *playlist = nullptr;
    bool exit = false;

    VideoContext *videoContext = nullptr;

    uint32_t dst_video_stream_index = 0;
    uint32_t dst_audio_stream_index = 1;
private:
    std::string outdir;
    const std::map<std::string, std::any> *extends_args;

    std::string streamId;


    SwrContext *swrContext = nullptr;

    AVFormatContext* new_output_context(const char* url, const std::vector<AVStream*>& streams);

public:
    HlsMuxer(const char*  url, const char*  outdir, const std::map<std::string, std::any> * ext_args);

    int start();

    // decode frames and push to fifo
    int decode_audio_packet(AVPacket *pkt);
    // write frames from fifo
    int write_encode_audio_packet();

    void set_packet_pts_dts(AVPacket *pkt, int frame_index);

//    int write_convert_packet(AVFormatContext *outputFormatContext, AVPacket *pkt);

    [[maybe_unused]] static int getStreamType(const std::string& url);

    int new_index_file();

    M3uSegment* new_seg_file(int segment_index, double_t duration);

    M3uSegment* get_segment(int index) const;

    static std::string new_seg_file_name(int segment_index);

    int write_playlist(int type);

    std::string write_playlist_header(int sequence = 0) const;

    static std::string write_playlist_file_entry(const M3uSegment* seg);

    std::string write_playlist_file_entries(int start = 0);

    static std::string write_playlist_file_end();

    void send_status(int action);

    void seg_clear();

    void close();
};

#endif //VIDEOPLAYER_HLS_H
