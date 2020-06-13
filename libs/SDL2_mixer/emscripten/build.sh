#!/bin/bash
set -e

# SAMPLE BUILD SCRIPT
# SDL2_MIXER + FLUIDSYNTH

# Source: https://github.com/emscripten-core/emscripten/compare/master...mazmazz:ports-fluidsynth

mkdir -p ~/workspace
cd ~/workspace

# Activate EMSDK
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
cd ..

# Clone ports-fluidsynth
git clone https://github.com/mazmazz/emscripten.git
cd emscripten
git checkout ports-fluidsynth

# Trigger library build via test suite
# (you may need to Ctrl+C after the builds are finished)
cd tests
python3 runner.py browser.tests_sdl2_mixer_midi

# Grab files from emscripten cache
cd ~/workspace/emsdk/upstream/emscripten/cache

# Package ports cache
# This directory contains source files. These are not necessary to use
# unless trying to slip the binaries into the emscripten cache

#7z a sdl2_mixer_fluidsynth.7z ports/fluidsynth/*
#7z a sdl2_mixer_fluidsynth.7z ports/sdl2_mixer/*
#7z a sdl2_mixer_fluidsynth.7z ports/fluidsynth.zip
#7z a sdl2_mixer_fluidsynth.7z ports/sdl2_mixer.zip

# Package WASM cache
# This directory contains the binaries and includes

7z a sdl2_mixer_fluidsynth.7z wasm/libSDL2_mixer.a
7z a sdl2_mixer_fluidsynth.7z wasm/libfluidsynth.a
7z a sdl2_mixer_fluidsynth.7z wasm/include/fluidsynth/*
7z a sdl2_mixer_fluidsynth.7z wasm/include/include/* # all fluidsynth
7z a sdl2_mixer_fluidsynth.7z wasm/include/SDL2/SDL_mixer.h
7z a sdl2_mixer_fluidsynth.7z wasm/ports-builds/fluidsynth/*
7z a sdl2_mixer_fluidsynth.7z wasm/ports-builds/sdl2_mixer/*

mv -f sdl2_mixer_fluidsynth.7z ~/workspace/sdl2_mixer_fluidsynth.7z

cd ~/workspace
