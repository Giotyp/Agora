#!/bin/bash

FILE="files/config/ci/tddconfig-sim-ul.json"
sudo LD_LIBRARY_PATH=${LD_LIBRARY_PATH} ./build/agora --conf_file=$FILE