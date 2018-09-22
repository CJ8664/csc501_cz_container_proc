#!/bin/bash

cd kernel_module
sudo make
sudo make install
read -p "Press any key..."
cd ..
sudo dmesg -C
./test.sh 1 2
sudo dmesg
