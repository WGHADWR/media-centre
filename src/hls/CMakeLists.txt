cmake_minimum_required(VERSION ${CMAKE_MINIMUM_VERSION})

project(hlsMedia VERSION 0.1 LANGUAGES C CXX)

aux_source_directory(. SOURCE_DIR)

message(STATUS "HLS_SOURCE_DIR = ${SOURCE_DIR}")

add_executable(hlsMedia
        ${SOURCE_DIR}
        ${UTILS_SOURCE_FILES}

        ${AV_UTILS_SOURCE_DIR}/video_context.h
        ${PROJECT_SOURCE_DIR}/../http/httpcli.h
        )

target_link_libraries(hlsMedia PRIVATE
        ${CURL_LIBRARIES}
        ${FFMpeg_LIBRARIES}
        ${EXT_LIBRARIES}
        )
