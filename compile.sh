#!/bin/sh
gcc rusty.c mpc.c -std=gnu99 -Wall -g -o rusty -Wno-format-truncation
