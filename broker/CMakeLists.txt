cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(broker)

# Disable thread-safe statics for C++
set(NO_THREADSAFE_STATICS $<TARGET_PROPERTY:compiler-cpp,no_threadsafe_statics>)
zephyr_compile_options($<$<COMPILE_LANGUAGE:CXX>:${NO_THREADSAFE_STATICS}>)

# Add all .c and .cpp sources
file(GLOB_RECURSE app_sources src/*.c src/*.cpp)

# Link them into the app
target_sources(app PRIVATE ${app_sources})

# Add include paths
target_include_directories(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/src/model
)

# Optional: include Zephyr network common
include(${ZEPHYR_BASE}/samples/net/common/common.cmake)
