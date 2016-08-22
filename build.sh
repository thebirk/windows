#!/bin/bash
gcc -std=c11 -g main.c mpack.c -lpthread -lm -lSDL2 -o windows