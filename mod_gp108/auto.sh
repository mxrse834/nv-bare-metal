#!/bin/bash
set -x

echo "Removing..."
sudo rmmod temp_c

echo "Building..."
make

echo "Loading..."
sudo insmod temp_c.ko

echo "KERNEL LOG"
sudo dmesg | tail -10

echo "Done"
