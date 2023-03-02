//
// Created by cyy on 2023/1/7.
//

#include "schedule.h"

bool HlsAllocator::expired(uint32_t timeout) const {
    auto now = GET_TIME_NOW_MILLISEC;
    auto diff = now - this->last_access_time;
    return diff / 1000 / 60 > timeout;
}

HlsAllocator* Schedule::get(std::string& id) {
    if (this->hlsProcessors.find(id) == this->hlsProcessors.end()) {
        return nullptr;
    }
    return this->hlsProcessors[id];
}

void Schedule::add(HlsAllocator* allocator) {
    if (this->hlsProcessors.find(allocator->id) != this->hlsProcessors.end()) {
        this->updateLastAccessTime(allocator->id);
        return;
    }
    auto now = GET_TIME_NOW_MILLISEC;
    allocator->last_access_time = now;
    this->hlsProcessors[allocator->id] = allocator;
}

void Schedule::updateLastAccessTime(std::string& id) {
    if (this->hlsProcessors.find(id) == this->hlsProcessors.end()) {
        return;
    }
    auto now = GET_TIME_NOW_MILLISEC;
    this->hlsProcessors[id]->last_access_time = now;
}

void Schedule::updateProcessStatus(std::string& id, uint32_t pid) {
    if (this->hlsProcessors.find(id) == this->hlsProcessors.end()) {
        return;
    }
    auto now = GET_TIME_NOW_MILLISEC;
    this->hlsProcessors[id]->last_living_time = now;
    this->hlsProcessors[id]->pid = pid;
}

void Schedule::remove(std::string& id) {
    this->hlsProcessors.erase(id);
}

bool Schedule::has_expired(std::string& id) {
    if (this->hlsProcessors.find(id) == this->hlsProcessors.end()) {
        return true;
    }
    auto proc = this->hlsProcessors[id];
    return proc->expired(this->timeout);
}

bool Schedule::running(std::string& id) {
    return this->hlsProcessors.find(id) != this->hlsProcessors.end();
}

[[noreturn]] void check_session(Schedule& schedule) {
    sm_log("Start hls session scheduler");
    while (true) {
        std::vector<std::string> expiredKeys;
        for (const auto &item: schedule.hlsProcessors) {
            if (!item.second || item.second->expired(schedule.timeout)) {
                expiredKeys.push_back(item.first);
            }
        }

        while (!expiredKeys.empty()) {
            auto iter = expiredKeys.begin();
            schedule.hlsProcessors.erase(*iter);
            expiredKeys.erase(iter);
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
        sm_error("Start session schedule failed; {}", e.what());
    }
    this->is_running = true;
    this->mutex.unlock();
}