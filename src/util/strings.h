//
// Created by cyy on 2023/1/9.
//

#ifndef MEDIACENTRE_STRINGS_H
#define MEDIACENTRE_STRINGS_H

#include <string>

static std::string toLower(const std::string& str) {
    std::string val;
    for (const auto &item: str) {
        val = val + (char)std::tolower(item);
    }
    return val;
}

#endif //MEDIACENTRE_STRINGS_H
