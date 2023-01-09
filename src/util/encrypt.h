//
// Created by cyy on 2023/1/5.
//

#ifndef VIDEOPLAYER_ENCRYPT_H
#define VIDEOPLAYER_ENCRYPT_H

#include <string>
#include "md5.h"

class Encrypt {

public:
    static std::string md5(std::string str) {
        return Md5(str);
    };

};

#endif //VIDEOPLAYER_ENCRYPT_H
