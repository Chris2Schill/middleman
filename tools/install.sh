#!/bin/bash -e

. ./environment.sh

cd $MIDDLEMAN_BUILD_HOME
cmake --install .
