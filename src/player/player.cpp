//
// Created by cyy on 2022/12/6.
//

#include "media-player.h"

int main(int argv, char *args[]) {
    MediaPlayer *player = new MediaPlayer;

    const char* url = args[1];
    if (argv <= 1) {
//        url = "C:\\Users\\cyy\\Desktop\\1.mp4";
//        url = "D:/Pitbull-Give-Me-Everything.mp4";
        url = "rtsp://192.168.2.46:8554/test1";
//        url = "rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mp4";
//        url = "http://playtv-live.ifeng.com/live/06OLEEWQKN4.m3u8";
//        url = "https://appletree-mytime-samsungbrazil.amagi.tv/playlist.m3u8";
    }
    sm_log("Input: %s\n", url);

    player->init(url);
    player->createWindow();

    return 0;
}

