cmake_minimum_required(VERSION ${CMAKE_MINIMUM_VERSION})

project(hlsMedia VERSION 0.1 LANGUAGES CXX)

aux_source_directory(. SOURCE_FILES)

message(STATUS "HLS_SOURCE_FILES = ${SOURCE_FILES}")

add_executable(hlsMedia
        ${UTILS_SOURCE_FILES}

        ${AV_UTILS_SOURCE_DIR}/video_context.h
        ${PROJECT_SOURCE_DIR}/../http/httpcli.h

        ${SOURCE_FILES}
        )

target_link_libraries(hlsMedia PUBLIC
        ${CURL_LIBRARIES}
        ${FFMpeg_LIBRARIES}
        ${EXT_LIBRARIES}
        util
        )
