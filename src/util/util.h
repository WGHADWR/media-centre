//
// Created by cyy on 2023/1/5.
//

#ifndef VIDEOPLAYER_UTIL_H
#define VIDEOPLAYER_UTIL_H

#include "encrypt.h"
#include "cmdline.h"
#include "strings.h"

#include <chrono>
#include <string>
//#include <map>
//#include <any>

#include "nlohmann/json.hpp"
#include "sm_log.h"


#define GET_TIME_NOW_MILLISEC (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())).count()

using nlojson = nlohmann::json;

static nlojson parse(const std::string& str) {
    nlojson data;
    try {
        data = nlojson::parse(str);
    } catch (nlojson::parse_error& e) {
        //return nullptr;
    }
    return data;
}


static std::string& replaceAll(std::string& str, std::string target, std::string replace) {
    if (str.empty()) {
        return str;
    }

    std::string::size_type pos(0);
    while (true) {
        pos = str.find(target, pos);
        if (pos == std::string::npos) {
            break;
        }
        str.replace(pos, target.length(), replace);
        pos += replace.length();
    }
    return str;
}

#endif //VIDEOPLAYER_UTIL_H
