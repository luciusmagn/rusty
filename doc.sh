#!/bin/sh
groff -W number -k -Tutf8 -dpaper=a4 -man rusty_cs.1 | less -r
