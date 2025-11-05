project(src)


set(SOURCES
    network/udp_transport.cpp
    network/middleman_proxy.cpp
    mutators/json_rule_based_mutator.cpp
    mutators/test_mutator.cpp
)

find_package(Boost REQUIRED)
find_package(spdlog REQUIRED)

add_library(mmcore STATIC ${SOURCES})
target_include_directories(mmcore PUBLIC ../include)
target_link_libraries(mmcore PUBLIC spdlog::spdlog)
if(WIN32)
    target_link_libraries(mmcore PUBLIC ws2_32)
endif()

add_subdirectory(cli)
add_subdirectory(gui)
