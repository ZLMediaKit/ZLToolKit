#!/bin/bash
ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
brew install cmake
cd ..
git clone --depth=50 https://github.com/xiongziliang/ZLToolKit.git
cd ZLToolKit
mkdir -p ios_build
rm -rf ./build
ln -s ./ios_build ./build
cd ios_build 
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/iOS.cmake -DIOS_PLATFORM=OS
make -j4
sudo make install
