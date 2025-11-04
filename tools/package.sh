#!/bin/bash -e

. ./environment.sh

cd $MIDDLEMAN_BUILD_HOME
cpack -G $CPACK_GENERATOR
