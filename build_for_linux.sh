#!/bin/bash
sudo apt-get install cmake
sudo apt-get install libmysqlclient-dev
sudo apt-get install libssl-dev
cd ..
git clone --depth=50 https://github.com/xiongziliang/ZLToolKit.git
cd ZLToolKit
mkdir -p linux_build
rm -rf ./build
ln -s ./linux_build ./build
cd linux_build
cmake ..
make -j4
sudo make install
