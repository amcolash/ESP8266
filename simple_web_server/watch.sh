#!/bin/sh
while inotifywait -e modify -e create -e delete . --recursive; do
  ./upload.bash
done
