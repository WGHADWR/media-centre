//
// Created by cyy on 2023/1/5.
//

#include "static_files_manager.h"

#include <fstream>
#include <cstdarg>

void StaticFilesManager::set_cache_status(bool open) {
    this->cache_status = open;
}

oatpp::String StaticFilesManager::getExtension(const oatpp::String& filename) {
    int dotPos = 0;
    for(auto i = filename->size() - 1; i > 0; i--) {
        if(filename->data()[i] == '.') {
            dotPos = (int)i;
            break;
        }
    }
    if(dotPos != 0 && dotPos < (int)filename->size() - 1) {
        return {(const char*)&filename->data()[dotPos + 1], (int)filename->size() - dotPos - 1};
    }
    return nullptr;
}

oatpp::String StaticFilesManager::getFile(const oatpp::String& path) {
    if(!path) {
        return nullptr;
    }
    std::lock_guard<oatpp::concurrency::SpinLock> lock(m_lock);
    auto file = (oatpp::String) "";
    if (this->cache_status) {
        file = m_cache[path];
        if (file) {
            return file;
        }
    }
    oatpp::String filename = m_basePath + "/" + path;
    file = oatpp::String::loadFromFile(filename->c_str());
    return file;
}

oatpp::String StaticFilesManager::guessMimeType(const oatpp::String& filename) {
    auto extension = getExtension(filename);
    if(extension) {
        if(extension == "html"){
            return "text/html";
        } else if(extension == "m3u8"){
            return "application/x-mpegURL";
        } else if(extension == "mp4"){
            return "video/mp4";
        } else if(extension == "ts"){
            return "video/MP2T";
        } else if(extension == "mp3"){
            return "audio/mp3";
        }

    }
    return nullptr;
}

oatpp::String formatText(const char* text, ...) {
    char buffer[4097];
    va_list args;
    va_start (args, text);
    vsnprintf(buffer, 4096, text, args);
    va_end(args);
    return oatpp::String(buffer);
}

v_int64 getMillisTickCount(){
    std::chrono::milliseconds millis = std::chrono::duration_cast<std::chrono::milliseconds>
            (std::chrono::system_clock::now().time_since_epoch());
    return millis.count();
}
