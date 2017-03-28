#!/bin/sh

# Uses the html-minifier package found at https://www.npmjs.com/package/html-minifier
html-minifier --remove-tag-whitespace --minify-css --collapse-whitespace index_full.html -o index.html
sudo nodemcu-uploader upload index.html

sudo nodemcu-uploader upload init.lua
sudo nodemcu-uploader upload server.lua

sudo nodemcu-uploader node restart
