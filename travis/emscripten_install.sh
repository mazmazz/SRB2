#!/bin/bash
set -ev

# Deployer for Travis-CI
# Emscripten Setup

# Save PWD to a file; for some reason, this var gets obliterated
echo "$PWD" > ~/oldpwd.txt

# Install latest emscripten
git clone https://github.com/emscripten-core/emsdk.git ~/emsdk
cd ~/emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh

# Install llvm 10 (current latest version)
wget https://apt.llvm.org/llvm.sh
chmod +x llvm.sh
sudo ./llvm.sh 10
export PATH="/usr/lib/llvm-10/bin:$PATH"

# Finishing touches
mkdir -p $HOME/srb2_cache
cd "$(<~/oldpwd.txt)"
