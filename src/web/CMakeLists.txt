cmake_minimum_required(VERSION ${CMAKE_MINIMUM_VERSION})

PROJECT(mediaServer VERSION 0.1 LANGUAGES C CXX)

aux_source_directory(. SOURCE_DIR)

message(STATUS "MEDIA_SERVER_SOURCE_DIR = ${SOURCE_DIR}")

add_definitions(
        -DWEB_STATIC_FOLDER="static"
)

add_executable(mediaServer
        ${UTILS_SOURCE_FILES}
        ${SOURCE_DIR}
        ./hls/HlsController.cpp
        )

target_include_directories(mediaServer PUBLIC
        ${YAML_CPP_INCLUDE_DIR}
)

target_link_libraries(mediaServer
        -Wl,-Bstatic
        oatpp
        -Wl,-Bstatic
        ${YAML_CPP_LIBRARIES}
        -Wl,-Bdynamic
        ${EXT_LIBRARIES}
        )

file(COPY media-server.yaml DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ../static DESTINATION ${CMAKE_BINARY_DIR})