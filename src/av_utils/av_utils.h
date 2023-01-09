//
// Created by cyy on 2022/12/16.
//

#ifndef VIDEOPLAYER_AV_UTILS_H
#define VIDEOPLAYER_AV_UTILS_H

#include <iostream>
#include <filesystem>
#include <string>
#include <sstream>
#include <ctime>
#include <chrono>
#include <iomanip>

extern "C" {
#include <time.h>
#include <io.h>
#include <direct.h>

#include "libavutil/avutil.h"
#include "libavutil/time.h"
};

static int create_dir(const char* path) {
    int ret = _access(path, 0);
    if (ret != -1) {
        return 0;
    }
    return std::filesystem::create_directories(path);
}

static const char* time_str(uint64_t millSec) {
    time_t t;
    struct tm *p;
    t = millSec / 1000;// + 28000;

    p = localtime(&t);
    static char s[100];
    strftime(s, sizeof(s), "%Y-%m-%d %H:%M:%S", p);
    return s;
}

static const char* time_utc_str(int64_t millSec) {
    auto time = std::chrono::milliseconds(millSec);
    auto tp = std::chrono::time_point<std::chrono::system_clock, std::chrono::microseconds>(time);
    auto tt = std::chrono::system_clock::to_time_t(tp);
    std::tm* tm = std::gmtime(&tt);

    auto ms = std::chrono::time_point_cast<std::chrono::milliseconds>(tp).time_since_epoch().count() % 1000;

    /*int year = tm->tm_year + 1900;
    int month = tm->tm_mon + 1;
    int day = tm->tm_mday;
    int hour = tm->tm_hour;
    int min = tm->tm_min;
    int sec = tm->tm_sec;*/
    //int ms = tp.time_since_epoch().count() / 1000;

    std::stringstream ss;
    static std::string str;
    ss << std::put_time(tm, "%Y-%m-%dT%H:%M:%S") << "." << ms << "Z";
    ss >> str;

    return str.c_str();
}


class Clock {
private:
    double_t pts = 0;
    double_t pts_drift = 0;
public:
    void setClock(double_t pts) {
        double_t time = av_gettime_relative() / AV_TIME_BASE;
        this->pts = pts;
        this->pts_drift = pts - time;
    }

    double_t getClock() {
        double_t time = av_gettime_relative() / AV_TIME_BASE;
        return this->pts_drift + time;
    }
};

#endif //VIDEOPLAYER_AV_UTILS_H
