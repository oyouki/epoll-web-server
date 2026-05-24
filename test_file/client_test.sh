#! /usr/bin/bash

cd ~/program/
cp client.c client_test.c
gcc -g -o client.out client_test.c ser_cli_std.c -lssl -lcrypto
gdb client.out