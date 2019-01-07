#!/bin/bash

git submodule init
git submodule update --remote

mkdir include
cp ./local/include/picosha2.h ./include
cp ./local/include/csv.h ./include
