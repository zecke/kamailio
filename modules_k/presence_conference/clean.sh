#!/bin/bash
exec &> /dev/null
make clean
rm -rf *.d
rm -rf *.o
rm -rf *~
