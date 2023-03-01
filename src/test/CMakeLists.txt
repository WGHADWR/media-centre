cmake_minimum_required(VERSION ${CMAKE_MINIMUM_VERSION})

project(test VERSION 0.1 LANGUAGES C CXX)

#aux_source_directory(. SOURCE_DIRS)

#message(STATUS "PROJECT_SOURCE_DIR = ${PROJECT_SOURCE_DIR} ${CMAKE_MINIMUM_VERSION}")

add_executable(test
        ${UTILS_SOURCE_FILES}
#        pcm_to_aac.cpp
#        ${SOURCE_DIRS}
        main.cpp)

#add_executable(transcode_aac
#        ${UTILS_SOURCE_FILES}
##        transcode_aac.cpp
#        #        ${SOURCE_DIRS}
#        )

target_link_libraries(test PRIVATE
#        ${CURL_LIBRARIES}
        ${FFMpeg_LIBRARIES}
        ${EXT_LIBRARIES}
        util
        #        ssl crypto crypt32
        )

#target_link_libraries(transcode_aac PRIVATE
#        #        ${CURL_LIBRARIES}
#        ${FFMpeg_LIBRARIES}
#        ${EXT_LIBRARIES}
#        #        ssl crypto crypt32
#        )