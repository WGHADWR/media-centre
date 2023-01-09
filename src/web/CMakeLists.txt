cmake_minimum_required(VERSION ${CMAKE_MINIMUM_VERSION})

PROJECT(mediaServer VERSION 0.1 LANGUAGES C CXX)

aux_source_directory(. SOURCE_DIR)

add_definitions(
        -DWEB_STATIC_FOLDER="static"
)

add_executable(mediaServer
        ${UTILS_SOURCE_FILES}
        ${SOURCE_DIR}
        ./hls/HlsController.cpp
        )

target_link_libraries(mediaServer
        -Wl,-Bstatic
        oatpp
        -Wl,-Bdynamic
        ${EXT_LIBRARIES}
        )

file(COPY ../static DESTINATION ${CMAKE_BINARY_DIR})