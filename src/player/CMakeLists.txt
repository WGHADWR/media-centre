cmake_minimum_required(VERSION ${CMAKE_MINIMUM_VERSION})

project(player VERSION 0.1 LANGUAGES C CXX)

aux_source_directory(. SOURCE_FILES)

message(STATUS "Media_Player_SOURCE_DIR = ${SOURCE_FILES}")

add_executable(player
        ${AV_UTILS_SOURCE_FILES}
        ${EXT_INCLUDE_DIRS}/spdlog/spdlog.h
        ${SOURCE_FILES}
        )

target_link_libraries(player PUBLIC
        ${SDL2_LIBRARIES}
        ${FFMpeg_LIBRARIES}
        )