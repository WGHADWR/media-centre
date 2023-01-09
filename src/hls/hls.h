//
// Created by cyy on 2023/1/3.
//

#ifndef VIDEOPLAYER_HLS_H
#define VIDEOPLAYER_HLS_H

#define __STDC_CONSTANT_MACROS

#include <vector>
#include <string>
#include "../av_utils/av_utils.h"
#include "../util/util.h"

#include "../http/httpcli.h"

extern "C" {
//#include <io.h>
//#include <direct.h>
#include "../av_utils/video_context.h"
};

/*
typedef unsigned char byte;

#define TS_PKT_SYNC_BYTE 0x47
#define TS_HEAD_PID_PAT 0x0000
#define TS_HEAD_PID_PMT 0x03e8

//typedef struct HLSHeader {
//    byte sync_byte[8]; // 8bit	同步字节，固定为0x47
//    byte transport_error_indicator; //	1bit	传输错误指示符，表明在ts头的adapt域后由一个无用字节，通常都为0，这个字节算在adapt域长度内
//    byte payload_unit_start_indicator; // 1bit	负载单元起始标示符，一个完整的数据包开始时标记为1
//    byte transport_priority; // 1bit	传输优先级，0为低优先级，1为高优先级，通常取0
//    byte pid[13]; // 13bit	pid值(Packet ID号码，唯一的号码对应不同的包)
//    byte transport_scrambling_control[2]; // 2bit	传输加扰控制，00表示未加密
//    byte adaptation_field_control[2]; // 2bit 是否包含自适应区，‘00’保留；‘01’为无自适应域，仅含有效负载；‘10’为仅含自适应域，无有效负载；‘11’为同时带有自适应域和有效负载。
//    byte continuity_counter[4]; // 递增计数器，从0-f，起始值不一定取0，但必须是连续的
//} HLSHeader;

typedef struct TS_HEAD {
    unsigned sync_byte: 8;  // 同步字节，固定为 0x47
    unsigned transport_error_indicator: 1;  // 传输错误指示符，0 表明在 TS 头的 adapt 域后没有无用字节
    unsigned payload_unit_start_indicator: 1;  // 负载单元起始标示符，一个完整的数据包开始时标记为 1
    unsigned transport_priority: 1;  // 传输优先级，0 为低优先级
    unsigned pid: 13; // PID 值，0 表示 TS 头后面就是 PAT 表
    unsigned transport_scrambling_controls: 2;  // TS 流 ID，一般为 0x0001，区别于一个网络中其它多路复用的流, 传输加扰控制，00表示未加密
    unsigned adaptation_field_control: 2;  // 是否包含自适应区，‘00’保留；‘01’为无自适应域，仅含有效负载；‘10’为仅含自适应域，无有效负载；‘11’为同时带有自适应域和有效负载。
    unsigned continuity_counter: 4;  // 递增计数器，0-F，起始值不一定取 0，但必须是连续的
} TS_HEAD;

typedef struct TS_ADAPT {
    unsigned char adaptation_field_length;  // 自适应域长度，后面的字节数，此包中为 7 字节
    unsigned char flag;                     // 0x50 表示包含 PCR，0x40 表示不包含 PCR
    // if (flag == 0x50)
    unsigned char PCR[5]; // 节目时钟参考 40bit
    std::vector<unsigned char> stuffing_bytes;   // 填充字节，取值 0xFF
} TS_ADAPT;

//typedef struct TS_PAT_Program {
//    unsigned program_number       :16;  // 节目号，为 0x0000 时表示这是 NIT，节目号为 0x0001 时, 表示这是 PMT
//    unsigned reserved             :3;   // 保留，固定为 111
//    unsigned network_PID      :13;
//    unsigned program_map_PID  :13;  // 节目号对应内容的 PID 值，该 PAT 包取值为 0x1000 = 4096
//} TS_PAT_Program;

typedef struct TS_PAT_Program {
    unsigned program_number       :16;  // 节目号，为 0x0000 时表示这是 NIT，节目号为 0x0001 时, 表示这是 PMT
    unsigned reserved             :3;    // 保留，固定为 111
    //unsigned network_PID          :13;  // network PID
    unsigned program_map_PID      :13;  // 节目号对应内容的 PID 值
} TS_PAT_Program;

typedef struct TS_PAT {
    unsigned table_id                     : 8;  // 固定 0x00 ，标志是该表是 PAT
    unsigned section_syntax_indicator     : 1;  // 段语法标志位，固定为 1
    unsigned zero                         : 1;  // 0
    unsigned reserved_1                   : 2;  // 保留位，固定为 11
    unsigned section_length               : 12; // 段长度，表示从下一个字段开始到 CRC32(含) 之间有用的字节数, 0000 00001101 值为 13
    unsigned transport_stream_id          : 16; // TS 流 ID，一般为 0x0001，区别于一个网络中其它多路复用的流
    unsigned reserved_2                   : 2;  // 保留位，固定为 11
    unsigned version_number               : 5;  // PAT 版本号，固定为 00000，如果 PAT 有变化则版本号加 1
    unsigned current_next_indicator       : 1;  // 固定为 1，表示这个 PAT 表有效，如果为 0 则要等待下一个 PAT 表
    unsigned section_number               : 8;  // 分段的号码。PAT 可能分为多段传输，第一段为 00，以后每个分段加 1，最多可能有 256 个分段
    unsigned last_section_number          : 8;  // 最后一个分段的号码
    std::vector<TS_PAT_Program> program;        // 该包中仅有一个 TS_PAT_Program
    unsigned CRC_32                       : 32; // CRC32 校验码
} TS_PAT;

typedef struct TS_PMT_Stream {
    unsigned stream_type              : 8;  // 指示本节目流的类型，H.264 编码对应 0x1b，AAC 编码对应 0x0f，MP3 编码对应 0x03
    unsigned reserved1                : 3;  // 保留位，固定为 111
    unsigned elementary_PID           : 13; // 指示该流的 PID 值
    unsigned reserved2                : 3;  // 保留位，固定为 1111
    unsigned ES_info_length           : 12; // 前两位 bit 为 00，指示跟随其后的描述相关节目元素的字节数
    std::vector<unsigned> descriptor;
} TS_PMT_Stream;

typedef struct TS_PMT {
    unsigned table_id                     : 8;  // 取值随意，一般使用 0x02, 表示 PMT 表
    unsigned section_syntax_indicator     : 1;  // 段语法标志位，固定为 1
    unsigned zero                         : 1;  // 固定为 0
    unsigned reserved_1                   : 2;  // 保留位，固定为 11
    unsigned section_length               : 12; // 段长度，表示从下一个字段开始到 CRC32(含) 之间有用的字节数
    unsigned program_number               : 16; // 当前 PMT 表映射到的节目号，1、2、3
    unsigned reserved_2                   : 2;  // 保留位，固定为 11
    unsigned version_number               : 5;  // PMT 版本号码，固定为 00000，如果 PAT 有变化则版本号加 1
    unsigned current_next_indicator       : 1;  // 发送的 PMT 表 是当前有效还是下一个 PMT 有效
    unsigned section_number               : 8;  // 分段的号码。PMT 可能分为多段传输，第一段为 00，以后每个分段加 1，最多可能有 256 个分段
    unsigned last_section_number          : 8;  // 分段数
    unsigned reserved_3                   : 3;  // 保留位，固定为 111
    unsigned PCR_PID                      : 13; // 指明 TS 包的 PID 值，该 TS 包含有 PCR 同步时钟，
    unsigned reserved_4                   : 4;  // 预留位，固定为 1111
    unsigned program_info_length          : 12; // 前 2bit 为 00，该域指出跟随其后对节目信息的描述的字节数。
    std::vector<TS_PMT_Stream> PMT_Stream;
    unsigned reserved_5                   : 3;  // 保留位，0x07
    unsigned reserved_6                   : 4;  // 保留位，0x0F
    unsigned CRC_32                       : 32; // CRC32 校验码
} TS_PMT;


typedef struct TS_PES {
    unsigned start_code_prefix        : 24; // 包头起始码，固定为 0x000001
    unsigned stream_id                : 8;  // PES 包中的负载流类型，一般视频为 0xe0，音频为 0xc0
    unsigned PES_packet_length        : 16; // PES 包长度，包括此字节后的可选包头和负载的长度
    unsigned reserved_1               : 2;  // 保留位，固定为 10
    unsigned PES_scrambling_control   : 2;  // 加密模式，00 未加密，01 或 10 或 11 由用户定义
    unsigned PES_priority             : 1;  // 有效负载的优先级，值为 1 比值为 0 的负载优先级高
    unsigned Data_alignment_indicator : 1;  // 数据定位指示器
    unsigned Copyright                : 1;  // 版权信息，1 为有版权，0 无版权
    unsigned Original_or_copy         : 1;  // 原始或备份，1 为原始，0 为备份
    unsigned PTS_DTS_flags            : 2;  // PTS 和 DTS 标志位，10 表示首部有 PTS 字段，11 表示有 PTS 和 DTS 字段，00 表示都没有，01 被禁止
    unsigned ESCR_flag                : 1;  // ESCR 标志，1 表示首部有 ESCR 字段，0 则无此字段
    unsigned ES_rate_flag             : 1;  // ES_rate 字段，1 表示首部有此字段，0 无此字段
    unsigned DSM_trick_mode_flag      : 1;  // 1 表示有 8 位的 DSM_trick_mode_flag 字段，0 无此字段
    unsigned Additional_copy_info_flag: 1;  // 1 表示首部有此字段，0 表示无此字段
    unsigned PES_CRC_flag             : 1;  // 1 表示 PES 分组有 CRC 字段，0 无此字段
    unsigned PES_extension_flag       : 1;  // 扩展标志位，置 1 表示有扩展字段，0 无此字段
    unsigned PES_header_data_length   : 8;  // PES 首部中可选字段和填充字段的长度，可选字段的内容由上面 7 个 flags 来进行控制
    \/*if (PTS_DTS_flags == '10') {
        unsigned reserved_1           : 4;  // 保留位，固定为 0010
        unsigned PTS_32_30            : 3;  // PTS
        unsigned marker_1             : 1;  // 保留位，固定为 1
        unsigned PTS_29_15            : 15; // PTS
        unsigned marker_2             : 1;  // 保留位，固定为 1
        unsigned PTS_14_0             : 15; // PTS
        unsigned marker_3             : 1;  // 保留位，固定为 1
    }
    if (PTS_DTS_flags == '11') {
        unsigned reserved_1           : 4;  // 保留位，固定为 0011
        unsigned PTS_32_30            : 3;  // PTS
        unsigned marker_1             : 1;  // 保留位，固定为 1
        unsigned PTS_29_15            : 15; // PTS
        unsigned marker_2             : 1;  // 保留位，固定为 1
        unsigned PTS_14_0             : 15; // PTS
        unsigned marker_3             : 1;  // 保留位，固定为 1
        unsigned reserved_2           : 4;  // 保留位，固定为 0001
        unsigned DTS_32_30            : 3;  // DTS
        unsigned marker_1             : 1;  // 保留位，固定为 1
        unsigned DTS_29_15            : 15; // DTS
        unsigned marker_2             : 1;  // 保留位，固定为 1
        unsigned DTS_14_0             : 15; // DTS
        unsigned marker_3             : 1;  // 保留位，固定为 1
    }
    if (ESCR_flag) ...
    if (ES_rate_flag) ...
    ...
    if (PES_extension_flag) ...*\/
    std::vector<unsigned char> stuffing_bytes;   // 填充字段，固定为 0xFF，不得超过 32 字节
    std::vector<unsigned char> PES_packet_data_byte; // PES 包的负载数据
} TS_PES;

typedef struct TS_PACKET {
    TS_HEAD *ts_head;
    TS_ADAPT *ts_adapt;
    void* payload;
} TS_PACKET;


TS_PACKET* new_pat_packet();

TS_PACKET* new_pmt_packet();

*/

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
    std::vector<M3uSegment> segments;

    FILE *file;
} M3uPlaylist;

static const char* HLS_M3U_INDEX_FILE = "index.m3u8";
static const char* HLS_SEG_FILE_PREFIX = "seg_";

class HlsMuxer {
private:
    std::string outdir;
    const nlohmann::json *extends_args;

    std::string streamId;
    M3uPlaylist *playlist;

    bool exit;

    static AVFormatContext* new_output_context(const AVOutputFormat *ofmt, const char* url, std::vector<AVStream*> streams);

public:
    explicit HlsMuxer(const char* url, const char* outdir, const nlohmann::json* ext_args);

    int start();

    int new_index_file();

    M3uSegment* new_seg_file(int segment_index, double_t duration);

    M3uSegment* get_segment(int index);

    static std::string new_seg_file_name(int segment_index);

    int write_playlist_header();

    int write_playlist_file_entry(M3uSegment *seg);

    int write_playlist_file_end();

    void send_status(int action);

    void close();
};

#endif //VIDEOPLAYER_HLS_H
