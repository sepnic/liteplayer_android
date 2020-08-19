cmake_minimum_required(VERSION 3.4.1)

# cflags: compile paramters
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -std=gnu99 -g -Wall -Werror -DOS_ANDROID")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Werror -UNDEBUG -DOS_ANDROID")

set(JNILIBS_DIR "${CMAKE_SOURCE_DIR}/../../../jniLibs")

include_directories(${JNILIBS_DIR}/include)
include_directories(${JNILIBS_DIR}/include/liteplayer)

add_library(msgutils SHARED IMPORTED)
set_target_properties(msgutils PROPERTIES IMPORTED_LOCATION "${JNILIBS_DIR}/libs/${ANDROID_ABI}/libmsgutils.so")

add_library(liteplayer_core SHARED IMPORTED)
set_target_properties(liteplayer_core PROPERTIES IMPORTED_LOCATION "${JNILIBS_DIR}/libs/${ANDROID_ABI}/libliteplayer_core.so")

add_library(liteplayer_adapter SHARED IMPORTED)
set_target_properties(liteplayer_adapter PROPERTIES IMPORTED_LOCATION "${JNILIBS_DIR}/libs/${ANDROID_ABI}/libliteplayer_adapter.so")

add_library(liteplayer-jni SHARED liteplayer-jni.cpp)

# Include libraries needed for native-codec-jni lib
target_link_libraries(liteplayer-jni
        msgutils
        liteplayer_core
        liteplayer_adapter
        android
        log)
