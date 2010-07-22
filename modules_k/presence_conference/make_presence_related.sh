#!/bin/bash
#make clean
make -j 3
cd ../../parser
###gcc parse_event.c -o parse_event.o
cd ../modules_k/presence
#make clean
make -j 3
