#!/bin/bash
set -e

# SAMPLE BUILD SCRIPT
# SRB2 WEB

# Run this script to build from a fully blank slate.

# This builds the game binary and packages it into the HTML shell.
# The output is a `srb2-web.zip` which is ready for deployment.

PACKAGEVERSION=${1:-2.2.4}
ASSETPATH=${2:-https://github.com/mazmazz/SRB2/releases/download/SRB2_assets_220/srb2-2.2.4-assets.7z}
OPTIONALASSETPATH=${3:-https://github.com/mazmazz/SRB2/releases/download/SRB2_assets_220/srb2-2.2.4-optional-assets-em.7z}
LOWENDASSETPATH=${4:-https://github.com/mazmazz/SRB2/releases/download/SRB2_assets_220/srb2-2.2.4-lowend-assets.7z}

mkdir -p ~/workspace
cd ~/workspace

# Install emscripten
# https://emscripten.org/docs/getting_started/downloads.html
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh

# Get repo source
git clone https://github.com/mazmazz/SRB2.git
cd SRB2
git checkout emscripten-new

# Build
emmake make -C src/

# Download assets into staging folder
cd emscripten
mkdir -p data
wget ${ASSETPATH};
wget ${OPTIONALASSETPATH};
7z x ./$(basename "${ASSETPATH}") -o./data
7z x ./$(basename "${OPTIONALASSETPATH}") -o./data

# Run packaging script
python3 ./emscripten-package.py ${PACKAGEVERSION} --ewad music.dta

cd .. # repo_root

#emmake make -C src/ clean

# clean up bin/Emscripten/Release for the next build
rm -r bin/Emscripten/Release/*
mkdir -p bin/Emscripten/Release
echo "*.data" > bin/Emscripten/Release/.gitignore
echo "*.txt" >> bin/Emscripten/Release/.gitignore
echo "*.html" >> bin/Emscripten/Release/.gitignore
echo "*.js" >> bin/Emscripten/Release/.gitignore
echo "*.wasm" >> bin/Emscripten/Release/.gitignore
# clean up objs/Emscripten/SDL/Release for the next build
rm -r objs/Emscripten/SDL/Release/*
mkdir -p objs/Emscripten/SDL/Release
echo "# DON'T REMOVE" > objs/Emscripten/SDL/Release/.gitignore
echo "# This keeps the folder from disappearing" >> objs/Emscripten/SDL/Release/.gitignore

# Build lowend version
emmake make -C src/ NOASYNCIFY=1

# Download low-end assets into staging folder
cd emscripten
mkdir -p data-lowend
wget ${LOWENDASSETPATH}
7z x ./$(basename "${LOWENDASSETPATH}") -o./data-lowend

# Package lowend version and zip up
python3 ./emscripten-package.py ${PACKAGEVERSION}-lowend --package-versions ${PACKAGEVERSION} ${PACKAGEVERSION}-lowend \
--default-package-version ${PACKAGEVERSION} --base-version ${PACKAGEVERSION} --data-dir ./data-lowend \
--out-zip ./srb2-web.zip

cd ~/workspace
