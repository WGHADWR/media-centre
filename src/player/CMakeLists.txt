cmake_minimum_required(VERSION ${CMAKE_MINIMUM_VERSION})

project(player VERSION 0.1 LANGUAGES C CXX)

aux_source_directory(. SOURCE_DIR)

message(STATUS "Media_Player_SOURCE_DIR = ${SOURCE_DIR}")

add_executable(player
        ${AV_UTILS_SOURCE_FILES}
        ${SOURCE_DIR}
        ${EXT_INCLUDE_DIRS}/spdlog/spdlog.h
        )

target_link_libraries(player PUBLIC
        ${SDL2_LIBRARIES}
        ${FFMpeg_LIBRARIES}
        )