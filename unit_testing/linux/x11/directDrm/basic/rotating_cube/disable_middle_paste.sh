#!/bin/bash
while true; do
    xdotool selectwindow --sync 2>/dev/null || true
    sleep 0.1
done
