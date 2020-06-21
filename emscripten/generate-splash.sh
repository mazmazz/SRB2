#!/bin/bash

# Get directory of this script
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

# Prepare command line
__GRAPHIC=${1:-${DIR}/landing/assets/srb2logo.png}
__BACKGROUND=${2:-${DIR}/landing/assets/background.jpg}
mimetype=$(file -bN --mime-type "$__BACKGROUND")
content=$(base64 -w0 < "$__BACKGROUND")
__BACKGROUND_DATA="url('data:$mimetype;base64,$content')"

# Perform
npm install -g pwa-asset-generator
pwa-asset-generator "$__GRAPHIC" "${DIR}/landing/assets" --background "linear-gradient(90deg, rgba(0,0,0,0.25) 0%, rgba(0,0,0,0.60) 10%, rgba(0,0,0,0.60) 90%, rgba(0,0,0,0.25) 100%), $__BACKGROUND_DATA; background-size: cover; background-position: center" --type "png" --splash-only
