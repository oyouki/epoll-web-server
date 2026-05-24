#! /usr/bin/bash

cd ~/program/
cp server.c server_test.c
cp db.c db_test.c
gcc -g -o db.out db_test.c ser_cli_std.c -lssl -lcrypto -lmysqlclient
gcc -g -o server.out server_test.c ser_cli_std.c -lssl -lcrypto
gdb server.out