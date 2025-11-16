#!/bin/bash

rm $PWD/screenshots/*
rm $PWD/screenshots_png/*
rm $PWD/screenshots_logs/*

if [ -f "$PWD/screenshots/*.ppm" ]; then
	echo "./screenshots/*.png found"
	rm $PWD/screenshots/*.png
fi

if [ -f "$PWD/screenshots_png/*.png" ]; then
	echo "./screenshots/*.png found"
	rm $PWD/screenshots_png/*
fi
