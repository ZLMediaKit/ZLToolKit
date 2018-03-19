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
cmake .. -DOPENSSL_ROOT_DIR=/usr/local/Cellar/openssl/1.0.2j/
make -j4
sudo make install
