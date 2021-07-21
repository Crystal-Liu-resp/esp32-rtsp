#!/bin/bash

set -e

if [ ! -d "build" ];then
  mkdir build
else
  echo "enter build"
fi

cd build
cmake ..
make
./rtsp_test