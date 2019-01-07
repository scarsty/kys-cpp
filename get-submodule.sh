#!/bin/bash

git clone https://github.com/scarsty/common common
git clone https://github.com/scarsty/lib-collection local

mkdir include
cp ./local/include/picosha2.h ./include
cp ./local/include/csv.h ./include
