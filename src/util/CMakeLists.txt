cmake_minimum_required(VERSION ${CMAKE_MINIMUM_VERSION})

project(util VERSION 0.1 LANGUAGES C CXX)

aux_source_directory(. SOURCE_FILES)

message(STATUS "UTIL_SOURCE_FILES = ${SOURCE_FILES}")

add_library(util
        ${SOURCE_FILES}
        )
