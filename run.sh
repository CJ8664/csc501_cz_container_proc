#!/bin/bash

cd kernel_module
sudo make
sudo make install
cd ..
sudo dmesg -C
./test.sh 2 2 2
sudo dmesg
