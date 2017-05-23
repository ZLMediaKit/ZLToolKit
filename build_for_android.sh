#!/bin/bash
ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
brew install cmake
cd ..
git clone --depth=50 https://github.com/xiongziliang/ZLToolKit.git
cd ZLToolKit
mkdir -p build
cd build 
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/android.toolchain.cmake -DANDROID_NDK=$ANDROID_NDK_ROOT  -DCMAKE_BUILD_TYPE=Release  -DANDROID_ABI="armeabi" -DANDROID_NATIVE_API_LEVEL=android-9
make -j4
