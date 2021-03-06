#!/bin/bash
set -e

FRAME_FILE="frames.h"
WAVE_FILE="wave.c"

# generate video files
cd video
if [ ! -f "$FRAME_FILE" ]; then
  echo "Start to generate video files"
  mkdir -p pngs
  mkdir -p frames
  ffmpeg -i ../test.mp4 -r 25 pngs/hd_%3d.png -y

  for image in ./pngs/*.png; 
  do 
      name=$(basename $image)
      convert "$image" -strip -quality 30% "frames/${name%.png}.jpg"
      echo “image $image converted to frames/${name%.png}.jpg ”
  done

  rm -rf pngs
  du -k frames
  python3 generate.py
fi
cd ..

# generate audio files
cd audio
if [ ! -f "$WAVE_FILE" ]; then
  echo "Start to generate audio files"
  ffmpeg -i ../test.mp4 -f mp3 -vn test.mp3 -y
  python3 mp32pcm.py
fi
cd ..
