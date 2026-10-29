cmake_minimum_required(VERSION 3.16)
project(kerr_ringdown CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(kerr_ringdown
  src/main.cpp
  src/app.cpp
  src/plots.cpp
  src/signal.cpp
  src/qnm.cpp
  src/svg.cpp)

if(MSVC)
  target_compile_options(kerr_ringdown PRIVATE /W4 /utf-8)
else()
  target_compile_options(kerr_ringdown PRIVATE -Wall -Wextra)
endif()

# Copy the fonts next to the executable so the SVGs embed IBM Plex Mono.
add_custom_command(TARGET kerr_ringdown POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/fonts $<TARGET_FILE_DIR:kerr_ringdown>/fonts)
