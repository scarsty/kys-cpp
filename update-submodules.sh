#!/bin/bash

git submodule sync --recursive
git submodule update --init --recursive
git submodule update --remote --rebase --recursive

