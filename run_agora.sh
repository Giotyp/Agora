#! /bin/bash

rm -f data/performance_dosubcarrier.txt
rm -f data/performance_dysubcarrier.txt
nice -20 chrt -r 99 ./build/agora --conf_file data/tddconfig-sim-ul-distributed.json