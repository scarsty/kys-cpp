#!/bin/bash

rm common
if [ ! -d "./common" ]; then
    git clone https://github.com/scarsty/common
else
    cd common
    git pull
fi

