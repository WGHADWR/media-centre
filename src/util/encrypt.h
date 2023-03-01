//
// Created by cyy on 2023/1/5.
//

#ifndef VIDEOPLAYER_ENCRYPT_H
#define VIDEOPLAYER_ENCRYPT_H

#include <string>
#include "md5.h"

static inline std::string md5(const std::string& str) {
    MD5Digest md5(str);
    return md5.hexdigest();
}

#endif //VIDEOPLAYER_ENCRYPT_H
