cmake_minimum_required(VERSION 3.21)

project(httpcli VERSION 0.1 LANGUAGES C CXX)

aux_source_directory(. SOURCE_DIR)

if("${CURL_DIR}" EQUAL "")
    message(FATAL_ERROR "Cannot find CURL_DIR")
endif()

set(CURL_INCLUDE_DIRS "${CURL_DIR}/include")
set(CURL_LIBRARY_DIRS "${CURL_DIR}/lib")
set(CURL_LIBRARIES curl)

add_library(httpcli STATIC ${SOURCE_DIR})

include_directories(
        ${CURL_INCLUDE_DIRS}
)

target_link_libraries(httpcli PRIVATE
        {CURL_LIBRARIES}
        )