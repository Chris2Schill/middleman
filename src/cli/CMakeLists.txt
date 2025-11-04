cmake_minimum_required(VERSION 3.0...3.5)

project(mmcli)

find_package(spdlog REQUIRED)
find_package(Boost REQUIRED)

add_executable(mmcli main.cpp)
target_link_libraries(mmcli PRIVATE mmcore)
