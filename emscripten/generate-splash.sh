#!/bin/bash

# Get directory of this script
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

npm install -g pwa-asset-generator
pwa-asset-generator "$1" "${DIR}/landing/assets" --background "black" --type "png" --splash-only
