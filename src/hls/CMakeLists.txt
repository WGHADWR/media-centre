cmake_minimum_required(VERSION ${CMAKE_MINIMUM_VERSION})

project(hls VERSION 0.1 LANGUAGES CXX)

aux_source_directory(. SOURCE_FILES)

message(STATUS "HLS_SOURCE_FILES = ${SOURCE_FILES}")

add_executable(hls
        ${UTILS_SOURCE_FILES}

        ${AV_UTILS_SOURCE_DIR}/video_context.h
        ${PROJECT_SOURCE_DIR}/../http/httpcli.h

        ${SOURCE_FILES}
        status_mgr.cpp status_mgr.h)

target_link_libraries(hls PUBLIC
        ${CURL_LIBRARIES}
        ${FFMpeg_LIBRARIES}
        ${EXT_LIBRARIES}
        util
        )
