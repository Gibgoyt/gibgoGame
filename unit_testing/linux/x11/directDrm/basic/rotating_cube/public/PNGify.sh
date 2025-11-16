#!/bin/bash

if [ -d screenshots_png ]; then
	rm screenshots_png/*.ppm
fi

cp screenshots/* screenshots_png
cd screenshots_png
for f in *.ppm; do ffmpeg -i "$f" "${f%.ppm}.png"; done
rm *.ppm
