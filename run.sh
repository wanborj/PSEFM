#!/bin/bash

make clean
nice -n -20 make qemu 2>&1 > ./PSEFM_DATA/$1\_$2.data
