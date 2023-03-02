//
// Created by cyy on 2023/1/7.
//

#ifndef MEDIACENTRE_SCHEDULE_H
#define MEDIACENTRE_SCHEDULE_H

#include <iostream>
#include <string>
#include <map>
#include <thread>
#include <mutex>

#include "../util/util.h"

class HlsAllocator {
public:
    std::string id;
    std::string url;
    uint32_t pid;
    uint64_t create_at;
    uint64_t last_access_time = 0;
    uint64_t last_living_time = 0;

public:
    HlsAllocator(std::string& id, std::string& url): id(id), url(url) {
        this->create_at = GET_TIME_NOW_MILLISEC;
        this->last_access_time = this->create_at;
    }

    bool expired(uint32_t timeout) const;

    nlohmann::json toJson() {
        static nlohmann::json data;
        data["id"] = this->id;
        data["url"] = this->url;
        data["create_at"] = this->create_at;
        data["last_access_time"] = this->last_access_time;
        data["last_living_time"] = this->last_living_time;
        return data;
    }
};

class Schedule {

public:
    std::map<std::string, HlsAllocator*> hlsProcessors;

    uint32_t timeout = 5;
private:
    bool is_running = false;
    std::mutex mutex;

public:
    HlsAllocator* get(std::string& id);
    void add(HlsAllocator* allocator);
    void updateLastAccessTime(std::string& id);
    void remove(std::string& id);

    bool running(std::string& id);

    void updateProcessStatus(std::string& id, uint32_t pid);

    bool has_expired(std::string& id);

    void start();

    std::string getAllocatorsJsonStr() {
        nlohmann::json list = nlohmann::json::array();
        for (const auto &item: this->hlsProcessors) {
            list.push_back(item.second->toJson());
        }
        return list.dump();
    }
};


#endif //MEDIACENTRE_SCHEDULE_H
