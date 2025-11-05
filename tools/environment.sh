#!/bin/bash -e

function set_and_print_env() {
    export $1="$2"
    echo "$1=$2"
}

case "$OSTYPE" in
    msys*)
        set_and_print_env VCPKG_TARGET_TRIPLET x64-mingw-dynamic
        set_and_print_env VCPKG_DEFAULT_TRIPLET $VCPKG_TARGET_TRIPLET
        set_and_print_env VCPKG_DEFAULT_HOST_TRIPLET $VCPKG_TARGET_TRIPLET
        set_and_print_env CPACK_GENERATOR "NSIS;ZIP"
        ;;
    linux*)
        set_and_print_env VCPKG_TARGET_TRIPLET x64-linux
        set_and_print_env VCPKG_DEFAULT_TRIPLET $VCPKG_TARGET_TRIPLET
        set_and_print_env VCPKG_DEFAULT_HOST_TRIPLET $VCPKG_TARGET_TRIPLET
        set_and_print_env CPACK_GENERATOR "TGZ"
        ;;
esac

set_and_print_env CMAKE_GENERATOR     "Ninja"
set_and_print_env CMAKE_MAKE_PROGRAM   ninja
set_and_print_env MIDDLEMAN_HOME           $(pwd)/..
set_and_print_env MIDDLEMAN_BUILD_HOME     ${MIDDLEMAN_HOME}/build/${VCPKG_TARGET_TRIPLET}
set_and_print_env CMAKE_BUILD_TYPE     Debug
set_and_print_env CMAKE_INSTALL_PREFIX $MIDDLEMAN_HOME/install
