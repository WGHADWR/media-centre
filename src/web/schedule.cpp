//
// Created by cyy on 2023/1/7.
//

#include "schedule.h"

bool HlsAllocator::expired() {
    auto now = GET_TIME_NOW_MILLISEC;
    auto diff = now - this->last_access_time;
    return diff / 1000 / 60 > 5;
}

void Schedule::add(HlsAllocator* allocator) {
    if (this->hlsProcessors[allocator->id]) {
        this->updateLastAccessTime(allocator->id);
        return;
    }

    auto now = GET_TIME_NOW_MILLISEC;
    allocator->last_access_time = now;
    this->hlsProcessors[allocator->id] = allocator;
}

void Schedule::updateLastAccessTime(std::string& id) {
    if (!this->hlsProcessors[id]) {
        return;
    }
    auto now = GET_TIME_NOW_MILLISEC;
    this->hlsProcessors[id]->last_access_time = now;
}

void Schedule::remove(std::string& id) {
    this->hlsProcessors.erase(id);
}

bool Schedule::running(std::string& id) {
    return this->hlsProcessors.find(id) != this->hlsProcessors.end();
}

void check_session(Schedule& schedule) {
    std::cout << "Start hls session scheduler" << std::endl;
    while (1) {
        std::vector<std::string> expiredKeys;
        for (const auto &item: schedule.hlsProcessors) {
            if (!item.second || item.second->expired()) {
                expiredKeys.push_back(item.first);
            }
        }

        while (!expiredKeys.empty()) {
            auto iter = expiredKeys.begin();
            schedule.hlsProcessors.erase(*iter);
            iter = expiredKeys.erase(iter);
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void Schedule::start() {
    this->mutex.lock();
    if (this->is_running) {
        this->mutex.unlock();
        return;
    }
    try {
        std::thread th(check_session, std::ref(*this));
        th.detach();
    } catch (std::exception& e) {
        std::cout << "Start session schedule failed; " << e.what() << std::endl;
    }
    this->is_running = true;
    this->mutex.unlock();
}