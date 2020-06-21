#!/bin/bash
set -ev

# Deployer for Travis-CI
# Emscripten Build Script

################################
# Preparation
################################

# Activate emsdk
source ~/emsdk/emsdk_env.sh

# Stage low-end assets
__ASSET_DIRECTORY="$TRAVIS_BUILD_DIR/emscripten/data"
if [[ "$ASSET_ARCHIVE_PATH_LOWEND" != "" ]]; then
    wget --verbose --server-response -N "$ASSET_ARCHIVE_PATH_LOWEND";
    7z x "$(basename $ASSET_ARCHIVE_PATH_LOWEND)" -o"${__ASSET_DIRECTORY}-lowend" -aos;
fi

# Prepare packaging variables
if [[ "$GTAG" != "" ]]; then
    GTAG="--gtag $GTAG";
fi
if [[ "$PACKAGE_VERSION" == "" ]]; then
    PACKAGE_VERSION="${TRAVIS_COMMIT:0:8}";
    PACKAGE_FILENAME="-${TRAVIS_JOB_ID}";
else
    PACKAGE_FILENAME="-${TRAVIS_COMMIT:0:8}-${TRAVIS_JOB_ID}";
fi
if [[ "$BASE_VERSION" != "" ]]; then
    BASE_VERSION="--base-version ${BASE_VERSION}";
else
    BASE_VERSION="--base-version ${PACKAGE_VERSION}";
fi
if [[ "$MAINTAINER" != "" ]]; then
    MAINTAINER="--maintainer ${MAINTAINER}";
fi
if [[ "$MAINTAINER_URL" != "" ]]; then
    MAINTAINER_URL="--maintainer-url ${MAINTAINER_URL}";
fi

# Prepare for build
cd $TRAVIS_BUILD_DIR
if [[ "${BUILD}" == "Debug" ]]; then
    DEBUGCMD="DEBUGMODE=1";
fi

################################
# Regular Build
################################

# Recall regular build OBJ
# rm -rf "$TRAVIS_BUILD_DIR/objs/Emscripten/SDL/${BUILD}";
# mkdir -p "$TRAVIS_BUILD_DIR/objs/Emscripten/SDL/${BUILD}";
# if [[ -d "$HOME/srb2_cache/em-objs-normal/${BUILD}" ]]; then
#     cp -r "$HOME/srb2_cache/em-objs-normal/${BUILD}" "$TRAVIS_BUILD_DIR/objs/Emscripten/SDL/${BUILD}";
# fi

# Make regular build
emmake make -C src/ $DEBUGCMD

# Cache regular build OBJ
# rm -rf "$HOME/srb2_cache/em-objs-normal/${BUILD}";
# mkdir -p "$HOME/srb2_cache/em-objs-normal/${BUILD}";
# cp -r "$TRAVIS_BUILD_DIR/objs/Emscripten/SDL/${BUILD}" "$HOME/srb2_cache/em-objs-normal/${BUILD}/"

# Package landing for regular build
python3 emscripten/emscripten-package.py ${PACKAGE_VERSION} \
    --package-versions ${PACKAGE_VERSION} ${PACKAGE_VERSION}-lowend \
    --default-package-version ${PACKAGE_VERSION} ${BASE_VERSION} ${GTAG} ${MAINTAINER} ${MAINTAINER_URL} \
    --build-dir "bin/Emscripten/${BUILD}" --data-dir "$__ASSET_DIRECTORY" --ewad music.dta

################################
# Low-End Build
################################

# Recall low-end build OBJ
# rm -rf "$TRAVIS_BUILD_DIR/objs/Emscripten/SDL/${BUILD}";
# mkdir -p "$TRAVIS_BUILD_DIR/objs/Emscripten/SDL/${BUILD}";
# if [[ -d "$HOME/srb2_cache/em-objs-lowend/${BUILD}" ]]; then
#     cp -r "$HOME/srb2_cache/em-objs-lowend/${BUILD}" "$TRAVIS_BUILD_DIR/objs/Emscripten/SDL/${BUILD}";
# fi

# Make low-end build
emmake make -C src/ NOASYNCIFY=1 $DEBUGCMD

# Cache low-end build OBJ
# rm -rf "$HOME/srb2_cache/em-objs-lowend/${BUILD}";
# mkdir -p "$HOME/srb2_cache/em-objs-lowend/${BUILD}";
# cp -r "$TRAVIS_BUILD_DIR/objs/Emscripten/SDL/${BUILD}" "$HOME/srb2_cache/em-objs-lowend/${BUILD}"

# Generate no-assets ZIP without GTAG or maintainer
python3 emscripten/emscripten-package.py ${PACKAGE_VERSION}-lowend \
    --package-versions ${PACKAGE_VERSION} ${PACKAGE_VERSION}-lowend \
    --default-package-version ${PACKAGE_VERSION} ${BASE_VERSION} \
    --build-dir "bin/Emscripten/${BUILD}" --data-dir "${__ASSET_DIRECTORY}-lowend" \
    --out-zip-no-assets "emscripten/srb2-web-${PACKAGE_VERSION}${PACKAGE_FILENAME}-no-assets.zip"

# Generate full ZIP with GTAG and maintainer
python3 emscripten/emscripten-package.py ${PACKAGE_VERSION}-lowend \
    --package-versions ${PACKAGE_VERSION} ${PACKAGE_VERSION}-lowend \
    --default-package-version ${PACKAGE_VERSION} ${BASE_VERSION} ${GTAG} ${MAINTAINER} ${MAINTAINER_URL} \
    --skip-build --skip-data \
    --out-zip "emscripten/srb2-web-${PACKAGE_VERSION}${PACKAGE_FILENAME}.zip"

# Reset current dir
cd $TRAVIS_BUILD_DIR/build
