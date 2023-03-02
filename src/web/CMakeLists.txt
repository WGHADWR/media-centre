cmake_minimum_required(VERSION ${CMAKE_MINIMUM_VERSION})

PROJECT(mediaServer VERSION 0.1 LANGUAGES C CXX)

aux_source_directory(. SOURCE_DIR)

message(STATUS "MEDIA_SERVER_SOURCE_DIR = ${SOURCE_DIR}")

add_definitions(
        -DWEB_STATIC_FOLDER="static"
)

add_executable(mediaServer
        ${UTILS_SOURCE_FILES}
        ./hls/HlsController.cpp
        ${SOURCE_DIR}
        )

target_include_directories(mediaServer PUBLIC
        ${YAML_CPP_INCLUDE_DIR}
)

target_link_libraries(mediaServer
        ${oatpp_LIBRARIES}
        ${YAML_CPP_LIBRARIES}
        ${EXT_LIBRARIES}
        )

add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${PROJECT_ROOT_DIR}/src/static ${CMAKE_BINARY_DIR}
)

add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${PROJECT_ROOT_DIR}/src/web/media-server.yaml ${CMAKE_BINARY_DIR}
)

file(COPY media-server.yaml DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ../static DESTINATION ${CMAKE_BINARY_DIR})