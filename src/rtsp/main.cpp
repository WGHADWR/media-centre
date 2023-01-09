
#ifdef WIN32
//#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

//#include "http/httplib.h"

//#include "media-player.h"
#include <cstdio>
#include "rtsp_push.h"
#undef main

static FILE *fp = nullptr;

static void global_log_callback(void *ptr, int level, const char *fmt, va_list vl) {
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

int main(int argv, char *args[]) {

    av_log_set_callback(global_log_callback);

    const char* url = args[1];
    const char* target = nullptr;
    if (argv <= 2) {
        if (!url) {
            url = "https://appletree-mytime-samsungbrazil.amagi.tv/playlist.m3u8";
                    //rtmp://media3.scctv.net/live/scctv_800";
                    //"http://devimages.apple.com.edgekey.net/streaming/examples/bipbop_4x3/gear2/prog_index.m3u8";
                    //"rtmp://ns8.indexforce.com/home/mystream";
                    //rtsp://192.168.56.119:8554/stream1";
//             url = "D:/Pitbull-Give-Me-Everything.mp4";
            url = "rtsp://192.168.56.119:8554/live/test1";
//           url = "C:\\Users\\cyy\\Desktop\\1.mp4";
//           url = "rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mp4";
        }
        target = "rtsp://192.168.56.119:8554/live/stream";
    } else {
        target = args[2];
    }
    printf("Input %s\n", url);
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

    /*httplib::Server serv;

    serv.Get("/about", [](const httplib::Request &, httplib::Response &resp) {
        resp.set_content("v0.0.1", "text/plain");
    });
    serv.set_error_handler([](const httplib::Request & *//*req*//*, httplib::Response &res) {
        const char *fmt = "<p>Error Status: <span style='color:red;'>%d</span></p>";
        char buf[BUFSIZ];
        snprintf(buf, sizeof(buf), fmt, res.status);
        res.set_content(buf, "text/html");
    });

    cout << "Start http server" << endl;
    int ret = serv.listen("0.0.0.0", 8080);
    if (!ret) {
        cout << "Start http server failed" << endl;
    }*/
    return 0;
}
