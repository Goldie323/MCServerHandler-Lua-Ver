#!/bin/bash

# Build Lua from source first
echo "Building Lua from source..."
cd include/lua-5.4.8/src
make clean
make linux
cd ../../../

# Copy the Lua interpreter to the main directory
echo "Copying Lua interpreter..."
cp include/lua-5.4.8/src/lua ./lua

# Compile PlatformTools as a shared library that Lua can load
echo "Compiling PlatformTools shared library..."
gcc -shared -fPIC -o platform.so PlatformTools.c \
    -I./include \
    -I./include/lua-5.4.8/src

echo "Compilation complete!"
echo "To use: ./lua your_script.lua"
echo "In your Lua script, load the library with: local platform = require('platform')"