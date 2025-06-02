#!/bin/bash
gcc -fsanitize=address -g -o output main.c -I/usr/include/freetype2 -lfreetype -lm

