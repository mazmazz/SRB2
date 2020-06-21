#!/bin/bash
set -e

# SAMPLE BUILD SCRIPT
# FLUIDSYNTH

# Source: https://github.com/FluidSynth/fluidsynth/compare/master...mazmazz:emscripten-ports

mkdir -p ~/workspace
cd ~/workspace

# Activate EMSDK
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
cd ..

# Clone fluidsynth-emscripten
git clone https://github.com/mazmazz/fluidsynth-emscripten.git
cd emscripten
git checkout origin/emscripten-ports

# Configure make
emcmake cmake -B ./build -H ./install -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=./install -Denable-static-emlib=on

# Make and install
cd ./build
emmake make install -k

# Package install directory
cd ../install
7z a fluidsynth.7z *

mv -f fluidsynth.7z ~/workspace/fluidsynth.7z

cd ~/workspace
