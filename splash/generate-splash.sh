#!/bin/bash

OLDPWD="$PWD"
# Get directory of this script
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
cd "$DIR"

npm install -g pwa-asset-generator
pwa-asset-generator "${DIR}/T5RBTX.png" --background "black" --type "png" --splash-only

cd "$OLDPWD"
