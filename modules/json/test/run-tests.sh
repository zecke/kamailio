#!/bin/bash

cd ../../../
./kamailio -D 1 -E -f modules/json/test/json-test.cfg -m 256 -M 256
