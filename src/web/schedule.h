//
// Created by cyy on 2023/1/7.
//

#ifndef MEDIACENTRE_SCHEDULE_H
#define MEDIACENTRE_SCHEDULE_H

#include <string>
#include <map>
#include <chrono>

class HlsAllocator {
private:
    const std::string id;
    const std::string url;
    const uint64_t last_access_time = 0;

public:
    bool expired();
};

class Schedule {

private:
    std::map<std::string, HlsAllocator> hlsProcessors;

};


#endif //MEDIACENTRE_SCHEDULE_H
