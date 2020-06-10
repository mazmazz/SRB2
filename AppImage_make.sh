#!/bin/bash
set -e

# Make Linux AppImage program data
# https://docs.appimage.org/reference/appdir.html

# PWD: {repo_root}/build
# See deployer.sh for usage

__BUILD_DIR=${1:-$PWD}
__OUTPUT_FILENAME=${2:-$__PROGRAM_FILENAME.AppImage}
__PROGRAM_NAME=${PROGRAM_NAME:-Sonic Robo Blast 2}
__PROGRAM_DESCRIPTION=${PROGRAM_DESCRIPTION:-A 3D Sonic the Hedgehog fangame inspired by the original Sonic games on the Sega Genesis.}
__PROGRAM_FILENAME=${PROGRAM_FILENAME:-lsdlsrb2}

echo "SRB2 AppImage Packager"
echo "----------------------------------------"
echo "Usage: $(basename "$0") [build_dir] [output_name]"
echo ""
echo "Staging AppImage in $__BUILD_DIR."
echo "This should be your CMAKE build directory. If it is not, then change to that directory or specify that directory as an argument."
echo "CMAKE must have built the program before you run this script."
echo ""
echo "Make sure these Environment Variables are correct. If not, then enter: export PROGRAM_VARIABLE=value"
echo "PROGRAM_NAME: $__PROGRAM_NAME"
echo "PROGRAM_DESCRIPTION: $__PROGRAM_DESCRIPTION"
echo "PROGRAM_FILENAME: $__PROGRAM_FILENAME"

echo ""

cmake .. -DSRB2_ASSET_INSTALL=ON -DSRB2_DEBUG_INSTALL=OFF -DCMAKE_INSTALL_PREFIX="${__BUILD_DIR}/staging";
$__MAKE -k install;

# Define AppDir structure
mkdir -p AppDir/usr/bin
mkdir -p AppDir/lib
mkdir -p AppDir/usr/share/applications
mkdir -p AppDir/usr/share/icons/hicolor/256x256/apps

# Copy program data
echo "Packaging program data..."
echo "Assuming executable name $__PROGRAM_FILENAME"
cp -r staging/* AppDir/usr/bin/

# Copy required dependencies
__LDD_LIST=$(python3 "$__BUILD_DIR/../AppImage_prunedepends.py" "$__BUILD_DIR/bin/$__PROGRAM_FILENAME")
IFS=' ' read -r -a paths <<< "$__LDD_LIST"
for path in "${paths[@]}";
do
    if [ -f "$path" ]; then
        echo "Packaging dependency $(basename $path)...";
        cp "$path" AppDir/lib/;
    else
        echo "Dependency $(basename $path) not found";
    fi;
done;

cd AppDir

# Copy icons
echo "Packaging resources..."
cp ../../srb2.png ./usr/share/icons/hicolor/256x256/apps/$__PROGRAM_FILENAME.png
ln -s ./usr/share/icons/hicolor/256x256/apps/$__PROGRAM_FILENAME.png ./.DirIcon
ln -s ./usr/share/icons/hicolor/256x256/apps/$__PROGRAM_FILENAME.png ./$__PROGRAM_FILENAME.png

# Make desktop descriptor
cat > ./usr/share/applications/$__PROGRAM_FILENAME.desktop <<EOF
[Desktop Entry]
Type=Application
Name=${__PROGRAM_NAME}
Comment=${__PROGRAM_DESCRIPTION}
Icon=${__PROGRAM_FILENAME}
Exec=AppRun %F
Categories=Game;
EOF
ln -s ./usr/share/applications/$__PROGRAM_FILENAME.desktop ./$__PROGRAM_FILENAME.desktop

# Make entry point
echo -e \#\!$(dirname $SHELL)/sh >> ./AppRun
echo -e 'HERE="$(dirname "$(readlink -f "${0}")")"' >> ./AppRun
echo -e 'SRB2WADDIR=$HERE/usr/bin LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$HERE/lib exec $HERE/usr/bin/'$__PROGRAM_FILENAME' "$@"' >> ./AppRun
chmod +x ./AppRun

cd ..
mkdir package

# Package AppImage
echo "Packing AppImage $__OUTPUT_FILENAME"
wget "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage"
chmod a+x appimagetool-x86_64.AppImage
./appimagetool-x86_64.AppImage ./AppDir $__OUTPUT_FILENAME
