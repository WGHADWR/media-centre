cmake_minimum_required(VERSION ${CMAKE_MINIMUM_VERSION})

project(player VERSION 0.1 LANGUAGES C CXX)

aux_source_directory(. SOURCE_DIRS)

add_executable(player
        ${AV_UTILS_SOURCE_FILES}
        ${SOURCE_DIRS}
        )

target_link_libraries(player PUBLIC
        ${SDL2_LIBRARIES}
        ${FFMpeg_LIBRARIES}
        )