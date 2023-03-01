//
// Created by cyy on 2022/11/25.
//

#ifndef VIDEOPLAYER_SM_LOG_H
#define VIDEOPLAYER_SM_LOG_H

#include <iostream>
#include <string>
#include <chrono>

#include "spdlog/spdlog.h"

static std::string log_time() {
    auto now = std::chrono::system_clock::now();
    time_t t = std::chrono::system_clock::to_time_t(now);
    auto tm = std::localtime(&t);
    int y = tm->tm_year + 1900;
    int m = tm->tm_mon + 1;
    int d = tm->tm_mday;

    int h = tm->tm_hour;
    int mi = tm->tm_min;
    int s = tm->tm_sec;

    std::string str = "";
    str.append(std::to_string(y));
    str += "-" + (m < 10 ? "0" + std::to_string(m) : std::to_string(m));
    str += "-" + (d < 10 ? "0" + std::to_string(d) : std::to_string(d));
    str.append(" ");
    str.append(h < 10 ? "0" + std::to_string(h) : std::to_string(h));
    str.append(":");
    str.append(mi < 10 ? "0" + std::to_string(mi) : std::to_string(mi));
    str.append(std::to_string(tm->tm_min));
    str.append(":");
    str.append(s < 10 ? "0" + std::to_string(s) : std::to_string(s));

    return str;
}

/*template <typename T>
static void sm_print_(T t) {
    std::cout << t;
}

template <typename T, typename ... Args>
static void sm_print_(T first, Args ... rest) {
    std::cout << first;
    sm_print_<T>(rest...);
}*/

template <typename T, class ... Args>
static void sm_log(T fmt, Args&& ... args) {
//    auto time = log_time();
//    std::cout << sizeof...(args) << std::endl;
    spdlog::info(fmt, args...);
}

template <typename T, typename ... Args>
static void sm_error(T fmt, Args&& ... args) {
    spdlog::error(fmt, args...);
}

template <typename T, typename ... Args>
static void sm_warn(T fmt, Args&& ... args) {
    spdlog::warn(fmt, args...);
}

#endif //VIDEOPLAYER_SM_LOG_H
