#!/bin/bash -e

. ./environment.sh

./configure.sh

cd $MIDDLEMAN_BUILD_HOME
cmake --build . -j10
