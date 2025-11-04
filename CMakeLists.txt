cmake_minimum_required(VERSION 3.0...3.5)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS FALSE)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

project(middleman LANGUAGES CXX)


include_directories(include)

add_subdirectory(src)

configure_file(config/mm_config.json ${CMAKE_BINARY_DIR}/src/cli)
configure_file(config/dis_types.json ${CMAKE_BINARY_DIR}/src/cli)
configure_file(config/test_rules.json ${CMAKE_BINARY_DIR}/src/cli)
configure_file(config/test_rules2.json ${CMAKE_BINARY_DIR}/src/cli)

configure_file(config/mm_config.json ${CMAKE_BINARY_DIR}/src/gui)
configure_file(config/dis_types.json ${CMAKE_BINARY_DIR}/src/gui)
configure_file(config/test_rules.json ${CMAKE_BINARY_DIR}/src/gui)
configure_file(config/test_rules2.json ${CMAKE_BINARY_DIR}/src/gui)
