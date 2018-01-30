#!/bin/bash
ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
brew install cmake
brew install mysql
brew install openssl
cd ..
git clone --depth=50 https://github.com/xiongziliang/ZLToolKit.git
cd ZLToolKit
mkdir -p mac_build
rm -rf ./build
ln -s ./mac_build ./build
cd mac_build 
cmake .. -DOPENSSL_ROOT_DIR=../TcpProxyServer/Android_Project/ProxySDK/SDK/libs/${ANDROID_ABI}/ make -j4
sudo make install
