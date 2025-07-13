#!/bin/sh

BUILD_DIR="./tools/build"

if [ ! -d "$BUILD_DIR" ];
then
    mkdir $BUILD_DIR
fi

# cat
gcc -nostdlib -static -g -o "$BUILD_DIR/cat" ./tools/cat/cat.c ./tools/runtime/sys.c ./tools/runtime/start.c

#echo
gcc -nostdlib -static -g -o "$BUILD_DIR/echo" ./tools/echo/echo.c ./tools/runtime/sys.c ./tools/runtime/start.c
