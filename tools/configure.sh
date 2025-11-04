#!/bin/bash -e

. ./environment.sh

mkdir -p $MIDDLEMAN_BUILD_HOME
pushd $MIDDLEMAN_BUILD_HOME

if [ -z ${VCPKG_ROOT+x} ]; then
    echo "Error: Environment variable VCPKG_ROOT not specified. Please point it to your vcpkg root directory."
    exit 1
fi

if [ ! -f "$MIDDLEMAN_BUILD_HOME/build.ninja" ]; then
    cmake -G "$CMAKE_GENERATOR" \
          -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE \
          -DCMAKE_INSTALL_PREFIX=$CMAKE_INSTALL_PREFIX \
          -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
          -DCMAKE_PREFIX_PATH=/usr/lib/x86_64-linux-gnu/cmake \
          -DVCPKG_TARGET_TRIPLET="$VCPKG_TARGET_TRIPLET" \
          -DCMAKE_MAKE_PROGRAM=$CMAKE_MAKE_PROGRAM \
          --profiling-format=google-trace \
          --profiling-output=cmakeprofile.json \
          $MIDDLEMAN_HOME
fi
