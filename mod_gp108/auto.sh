#!/bin/bash
set -x

echo "Removing..."
sudo rmmod temp_P3

echo "Building..."
sudo make

echo "Loading..."
sudo insmod temp_P3.ko

echo "KERNEL LOG"
sudo dmesg | tail -10

echo "Done"
