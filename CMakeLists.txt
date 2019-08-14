cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)
project(render LANGUAGES C)

if(NOT TARGET_OS)
  string(TOLOWER ${CMAKE_SYSTEM_NAME} TARGET_OS)
endif()

# Library
add_library(render "./src/render.c")

if(TARGET_OS MATCHES "linux")
  target_compile_definitions(render PUBLIC TARGET_OS_LINUX)
elseif(TARGET_OS MATCHES "windows")
  target_compile_definitions(render PUBLIC TARGET_OS_WINDOW)
endif()

# Compile options
add_dependencies(render error window)
target_include_directories(render PUBLIC "./src")
target_link_libraries(render error window)
target_compile_options(render
  PUBLIC "-std=c90"
  PUBLIC "-pedantic-errors"
  PUBLIC "-Wall"
  PUBLIC "-Wconversion"
  )
