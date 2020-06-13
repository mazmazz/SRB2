#!/bin/bash
set -e

# WORKING BUILD SCRIPT
# SRB2 WEB

# Run this script when you want to work off the current repository
# and you already have EMSDK activated.

# This builds the game binary and packages it into the HTML shell.
# The output is a `srb2-web.zip` which is ready for deployment.

# Get directory of this script
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

# Vars
PACKAGEVERSION=${1:-2.2.4}
ASSETPATH=${2:-https://github.com/mazmazz/SRB2/releases/download/SRB2_assets_220/srb2-2.2.4-assets.7z}
OPTIONALASSETPATH=${3:-https://github.com/mazmazz/SRB2/releases/download/SRB2_assets_220/srb2-2.2.4-optional-assets-em.7z}
LOWENDASSETPATH=${4:-https://github.com/mazmazz/SRB2/releases/download/SRB2_assets_220/srb2-2.2.4-lowend-assets.7z}

cd $DIR/..

# Build
emmake make -C src/

# Download assets into staging folder
cd emscripten
mkdir -p data
if [ ! -f "${ASSETPATH}" ]; then
    wget ${ASSETPATH};
fi
if [ ! -f "${OPTIONALASSETPATH}" ]; then
    wget ${OPTIONALASSETPATH};
fi
7z x ./$(basename "${ASSETPATH}") -o./data
7z x ./$(basename "${OPTIONALASSETPATH}") -o./data

# Run packaging script for base version
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
if [ ! -f "${LOWENDASSETPATH}" ]; then
    wget ${LOWENDASSETPATH};
fi
7z x ./$(basename "${LOWENDASSETPATH}") -o./data-lowend

# Package lowend version and zip up
python3 ./emscripten-package.py ${PACKAGEVERSION}-lowend --package-versions ${PACKAGEVERSION} ${PACKAGEVERSION}-lowend \
--default-package-version ${PACKAGEVERSION} --base-version ${PACKAGEVERSION} --data-dir ./data-lowend \
--out-zip ./srb2-web.zip

cd ..

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
