cmake_minimum_required(VERSION ${CMAKE_MINIMUM_VERSION})

project(rtsp VERSION 0.1 LANGUAGES C CXX)

aux_source_directory(. SOURCE_DIRS)

add_executable(rtsp-push
        ${UTILS_SOURCE_FILES}
        ${AV_UTILS_SOURCE_DIR}/av_utils.h
        ${AV_UTILS_SOURCE_DIR}/av_packet_queue.h
        ${AV_UTILS_SOURCE_DIR}/av_packet_queue.cpp

        ${SOURCE_DIRS}
        )

target_link_libraries(rtsp-push PUBLIC
        ${SDL2_LIBRARIES}
        ${FFMpeg_LIBRARIES}
        ${EXT_LIBRARIES}
        util
        )
