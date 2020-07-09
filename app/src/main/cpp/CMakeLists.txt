cmake_minimum_required(VERSION 3.4.1)

# cflags: compile paramters
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -std=gnu99 -g -Wall -Werror -DOS_ANDROID")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Werror -UNDEBUG -DOS_ANDROID")

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/include/liteplayer)

add_library(msgutils SHARED IMPORTED)
set_target_properties(msgutils PROPERTIES IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/libs/${ANDROID_ABI}/libmsgutils.so")

add_library(liteplayercore SHARED IMPORTED)
set_target_properties(liteplayercore PROPERTIES IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/libs/${ANDROID_ABI}/libliteplayercore.so")

add_library(liteplayeradapter SHARED IMPORTED)
set_target_properties(liteplayeradapter PROPERTIES IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/libs/${ANDROID_ABI}/libliteplayeradapter.so")

add_library(liteplayer-jni SHARED liteplayer-jni.cpp)

# Include libraries needed for native-codec-jni lib
target_link_libraries(liteplayer-jni
        msgutils
        liteplayercore
        liteplayeradapter
        android
        log)
