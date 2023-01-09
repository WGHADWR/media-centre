set(CMAKE_MINIMUM_VERSION 3.21)

cmake_minimum_required(VERSION ${CMAKE_MINIMUM_VERSION})
project(MediaCentre VERSION 1.0)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 17)

set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}")
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}")

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(SDL2_DIR "/opt/SDL2/lib/cmake/SDL2")
    set(FFMpeg_PATH "/opt/ffmpeg")

    set(oatpp_DIR "/opt/oatpp/lib64/cmake/oatpp-1.3.0")

    set(EXT_LIBRARIES pthread)
else()
    set(CURL_DIR "D:/Application/msys64/opt/curl")

    set(SDL2_DIR "D:/Application/SDL2-2.26.1/cmake")
    set(FFMpeg_PATH "D:/Application/ffmpeg/ffmpeg-gpl-shared")

    set(oatpp_DIR "D:/Application/msys64/opt/oatpp/lib/cmake/oatpp-1.3.0")

    set(EXT_LIBRARIES wsock32 ws2_32)

    set( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-subsystem,console" )

#    if (${CMAKE_BUILD_TYPE} EQUAL "Release")
#        set( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-subsystem,console" )
#    else()
#        set( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-subsystem,windows" )
#    endif()
endif()

#find_package(CURL REQUIRED)

find_package(SDL2 REQUIRED)
find_package(oatpp REQUIRED)

set(FFMpeg_LIBRARIES avcodec avformat avutil swscale swresample)
set(FFMpeg_INCLUDE_DIRS "${FFMpeg_PATH}/include")
set(FFMpeg_LIBDIR ${FFMpeg_PATH}/lib)

set(CURL_INCLUDE_DIRS "${CURL_DIR}/include")
set(CURL_LIBRARY_DIRS "${CURL_DIR}/lib")
set(CURL_LIBRARIES curl)

set(CMAKE_EXE_LINKER_FLAGS "-L${FFMpeg_LIBDIR} -L${oatpp_LIBRARIES_DIRS} -L${CURL_LIBRARY_DIRS} ${CMAKE_EXE_LINKER_FLAGS}")

#set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};${SDL2_DIR}")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wwrite-strings")

set(CMAKE_INSTALL_PREFIX build)

set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} \
    -isystem ${FFMpeg_INCLUDE_DIRS} \
     -O0 -Wall -g2 -ggdb -lpthread \
    ")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} \
    -isystem ${FFMpeg_INCLUDE_DIRS} \
     -O3 -Wall -lpthread -static \
    ")


MESSAGE( STATUS "CMAKE_SYSTEM_NAME = ${CMAKE_SYSTEM_NAME}")
MESSAGE( STATUS "CMAKE_C_STANDARD = ${CMAKE_C_STANDARD}")
MESSAGE( STATUS "CMAKE_CXX_STANDARD = ${CMAKE_CXX_STANDARD}")
MESSAGE( STATUS "CMAKE_PREFIX_PATH = ${CMAKE_PREFIX_PATH}")
MESSAGE( STATUS "CMAKE_INSTALL_PREFIX = ${CMAKE_INSTALL_PREFIX}")
MESSAGE( STATUS "CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")
MESSAGE( STATUS "CMAKE_EXE_LINKER_FLAGS = ${CMAKE_EXE_LINKER_FLAGS}")

message( STATUS "PROJECT_BINARY_DIR = ${PROJECT_BINARY_DIR}")

MESSAGE( STATUS "SDL2_INCLUDE_DIRS = ${SDL2_INCLUDE_DIRS}")
MESSAGE( STATUS "SDL2_LIBDIR = ${SDL2_LIBDIR}")
MESSAGE( STATUS "SDL2_LIBRARIES = ${SDL2_LIBRARIES}")

MESSAGE( STATUS "FFMpeg_PATH = ${FFMpeg_PATH}")
MESSAGE( STATUS "FFMpeg_INCLUDE_DIRS = ${FFMpeg_INCLUDE_DIRS}")
MESSAGE( STATUS "FFMpeg_LIBDIR = ${FFMpeg_LIBDIR}")
MESSAGE( STATUS "FFMpeg_LIBRARIES = ${FFMpeg_LIBRARIES}")

MESSAGE( STATUS "oatpp_INCLUDE_DIRS = ${oatpp_INCLUDE_DIRS}")
MESSAGE( STATUS "oatpp_LIBRARIES_DIRS = ${oatpp_LIBRARIES_DIRS}")

MESSAGE( STATUS "CURL_DIR = ${CURL_DIR}")
MESSAGE( STATUS "CURL_INCLUDE_DIRS = ${CURL_INCLUDE_DIRS}")
MESSAGE( STATUS "CURL_LIBRARY_DIRS = ${CURL_LIBRARY_DIRS}")
MESSAGE( STATUS "CURL_LIBRARIES = ${CURL_LIBRARIES}")

#if (CMAKE_SYSTEM_NAME MATCHES "MSYS")
#    set(CMAKE_FIND_LIBRARY_SUFFIXES ".lib")
#elseif (UNIX)
#    # 仅查找静态库，强制后缀为 .a
#    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
#
#    # 如果只是优先查找静态库，保证 .a 后缀在前面即可，把默认的后缀加上
#    # set(CMAKE_FIND_LIBRARY_SUFFIXES .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
#endif()
#MESSAGE( STATUS "CMAKE_FIND_LIBRARY_SUFFIXES = ${CMAKE_SYSTEM_NAME}")

set(UTILS_SOURCE_FILES
        ${PROJECT_SOURCE_DIR}/src/util/encrypt.h
        ${PROJECT_SOURCE_DIR}/src/util/md5.h
        ${PROJECT_SOURCE_DIR}/src/util/md5.cpp
        ${PROJECT_SOURCE_DIR}/src/util/cmdline.h

        ${PROJECT_SOURCE_DIR}/src/util/nlohmann/json.hpp
        )

set(AV_UTILS_SOURCE_DIR ${PROJECT_SOURCE_DIR}/src/av_utils)
set(AV_UTILS_SOURCE_FILES
        ${AV_UTILS_SOURCE_DIR}/av_packet_queue.cpp
        ${AV_UTILS_SOURCE_DIR}/av_packet_queue.h
        ${AV_UTILS_SOURCE_DIR}/av_utils.h
        ${AV_UTILS_SOURCE_DIR}/sm_log.cpp
        ${AV_UTILS_SOURCE_DIR}/sm_log.h

        ${AV_UTILS_SOURCE_DIR}/video_context.h
        )

include_directories(SYSTEM
        ${CURL_INCLUDE_DIRS}
        ${FFMpeg_INCLUDE_DIRS}
        ${SDL2_INCLUDE_DIRS}
        ${oatpp_INCLUDE_DIRS}
        )

#if (CMAKE_SYSTEM_NAME MATCHES "Linux")
#    link_directories(${FFMpeg_LIBDIR})
#endif()

add_subdirectory(src/rtsp)
add_subdirectory(src/player)
add_subdirectory(src/hls)
add_subdirectory(src/web)
