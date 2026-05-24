#! /usr/bin/bash

cd ~/program/
cp tool.c tool_test.c
gcc -g -o tool_test.out tool_test.c
gdb tool_test.out