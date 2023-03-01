//
// Created by cyy on 2023/1/5.
//

#ifndef VIDEOPLAYER_STATIC_FILES_MANAGER_H
#define VIDEOPLAYER_STATIC_FILES_MANAGER_H


#include "oatpp/core/concurrency/SpinLock.hpp"
#include "oatpp/core/Types.hpp"

#include <unordered_map>
#include <string>

class StaticFilesManager {
private:
    oatpp::String m_basePath;
    oatpp::concurrency::SpinLock m_lock;
    std::unordered_map<oatpp::String, oatpp::String> m_cache;

    bool cache_status = true;
private:
    static oatpp::String getExtension(const oatpp::String& filename);
public:

    StaticFilesManager(const oatpp::String& basePath)
            : m_basePath(basePath)
    {}

    void set_cache_status(bool open);

    oatpp::String getFile(const oatpp::String& path);
    static oatpp::String guessMimeType(const oatpp::String& filename);

};

oatpp::String formatText(const char* text, ...);
v_int64 getMillisTickCount();


#endif //VIDEOPLAYER_STATIC_FILES_MANAGER_H
