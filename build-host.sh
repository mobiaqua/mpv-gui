#!/bin/sh

FC_CXXFLAGS=`pkg-config --cflags fontconfig`
FC_LDFLAGS=`pkg-config --libs fontconfig`
FT_CXXFLAGS=`pkg-config --cflags freetype2`
FT_LDFLAGS=`pkg-config --libs freetype2`
SDL2_CXXFLAGS=`pkg-config --cflags sdl2`
SDL2_LDFLAGS=`pkg-config --libs sdl2`
CURL_CXXFLAGS=`pkg-config --cflags libcurl`
CURL_LDFLAGS=`pkg-config --libs libcurl`

export CXX=g++
export CXXFLAGS="-std=c++17 -g3 -O0 -Isrc -DBUILD_SDL2 ${FC_CXXFLAGS} ${FT_CXXFLAGS} ${SDL2_CXXFLAGS} ${CURL_CXXFLAGS}"
export LDFLAGS="$FC_LDFLAGS ${FT_LDFLAGS} ${SDL2_LDFLAGS} ${CURL_LDFLAGS}"

export CXXFLAGS="$CXXFLAGS -fsanitize=address -fno-omit-frame-pointer"
export LDFLAGS="$LDFLAGS -fsanitize=address -fno-omit-frame-pointer"

make -j8
