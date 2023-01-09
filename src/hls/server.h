//
// Created by cyy on 2023/1/6.
//

#ifndef VIDEOPLAYER_HLS_SERVER_H
#define VIDEOPLAYER_HLS_SERVER_H

//#define CPPHTTPLIB_OPENSSL_SUPPORT

#include <vector>
#include "unistd.h"
#include <ws2tcpip.h>

#include "../util/util.h"
#include "../http/httpcli.h"

#include "hls.h"

class HlsSchedule {

private:
    std::vector<HlsMuxer> tasks;

public:
    void createTask(const char* url);
};


#endif //VIDEOPLAYER_SCHEDULE_H
