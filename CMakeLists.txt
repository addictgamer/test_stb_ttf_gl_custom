
cmake_minimum_required(VERSION 2.8)

project(stb_sdl2_test)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

#find_package(STB_TTF REQUIRED)
#INCLUDE_DIRECTORIES(${STB_TTF_INCLUDE_DIR})

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIR})
link_directories(${SDL2_LIBRARY})

find_package(OpenGL REQUIRED)

add_executable(test_stb src/main.cpp src/stb.cpp)
target_link_libraries(test_stb ${SDL2_LIBRARY})
target_link_libraries(test_stb ${OPENGL_LIBRARIES})
