//
// Created by cyy on 2023/1/5.
//

#ifndef VIDEOPLAYER_UTIL_H
#define VIDEOPLAYER_UTIL_H

#include "encrypt.h"
#include "cmdline.h"

#include "nlohmann/json.hpp"

using json = nlohmann::json;

static json parse(const std::string& str) {
    json data;
    try {
        data = json::parse(str);
    } catch (json::parse_error& e) {
    }
    return data;
}

#endif //VIDEOPLAYER_UTIL_H
