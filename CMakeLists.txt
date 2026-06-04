cmake_minimum_required(VERSION 3.5)
project(hach)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(third_party/EIPScanner)

add_executable(hach main.cpp)

target_include_directories(hach PRIVATE ${CMAKE_SOURCE_DIR}/third_party/EIPScanner/src)
target_link_libraries(hach EIPScanner)
