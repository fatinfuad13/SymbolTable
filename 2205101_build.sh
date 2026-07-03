#!/bin/bash

if [ "$#" -ne 2 ]; then
    echo "Usage: ./2205101_build.sh <input_file> <output_file>"
    exit 1
fi

g++ -fsanitize=address -g 2205101_symbol_table.cpp -o 2205101_symbol_table && ./2205101_symbol_table "$1" "$2"
