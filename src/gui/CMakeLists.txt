# cmake_minimum_required(VERSION 3.0...3.5)
#
# project(mmgui)
#
# find_package(Qt6 REQUIRED)
#
# add_executable(mmgui main.cpp)
# # target_link_directories(mmgui PRIVATE ${CMAKE_BINARY_DIR}/vcpkg_installed/x64-linux/lib)
# target_link_libraries(mmgui PRIVATE Qt6::Widgets)

cmake_minimum_required(VERSION 3.0..3.5)

project(mmgui VERSION 1.0.0 LANGUAGES CXX)

find_package(Qt6 COMPONENTS Widgets Network PrintSupport)
if (NOT Qt6_FOUND)
    find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets Network PrintSupport)
endif()

find_package(spdlog REQUIRED)
find_package(Boost REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(SOURCES
    main.cpp
    connection_widget.cpp
    rules_editor_widget.cpp
    udp_viewer_widget.cpp
)

add_executable(mmgui ${SOURCES})
# add_executable(mmgui gpt.cpp)
target_link_libraries(mmgui PRIVATE Qt6::Widgets Qt6::Network spdlog::spdlog mmcore)
target_include_directories(mmgui PRIVATE .)
set_target_properties(mmgui PROPERTIES
    # WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)

# install(TARGETS rtdbg DESTINATION bin)
# install(DIRECTORY ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/ DESTINATION bin)
install(IMPORTED_RUNTIME_ARTIFACTS mmgui RUNTIME_DEPENDENCY_SET mmgui_deps)
install(RUNTIME_DEPENDENCY_SET mmgui_deps
	PRE_EXCLUDE_REGEXES api-ms-
	DESTINATION bin
    DIRECTORIES /c/msys64/mingw64/bin C:\\\\msys64\\\\mingw64\\\\bin
)

# Trying desperately to reliably get all dependencies in the binary dir
if (WIN32)
    add_custom_command(TARGET mmgui POST_BUILD
        COMMAND Qt6::windeployqt
        ARGS $<TARGET_FILE:mmgui>
    )

    add_custom_command(TARGET mmgui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/
        $<TARGET_FILE_DIR:mmgui>
    )
endif()
