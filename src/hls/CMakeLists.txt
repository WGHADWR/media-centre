cmake_minimum_required(VERSION ${CMAKE_MINIMUM_VERSION})

project(hlsMedia VERSION 0.1 LANGUAGES C CXX)

aux_source_directory(. SOURCE_DIRS)

#message(STATUS "PROJECT_SOURCE_DIR = ${PROJECT_SOURCE_DIR} ${CMAKE_MINIMUM_VERSION}")

add_executable(hlsMedia
        ${UTILS_SOURCE_FILES}
        ${SOURCE_DIRS}

        ${AV_UTILS_SOURCE_DIR}/video_context.h
        ${PROJECT_SOURCE_DIR}/../http/httpcli.h
        )

target_link_libraries(hlsMedia PRIVATE
        ${CURL_LIBRARIES}
        ${FFMpeg_LIBRARIES}
        ${EXT_LIBRARIES}
        #        ssl crypto crypt32
        )
