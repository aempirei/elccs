#!/bin/bash

if [ -z "$1" ] || [ ! -c "$1" ]; then
	echo
	echo usage: $0 arduino_device
	echo
	exit
fi

# 115200 baud

stty -F $1 1:0:800018b2:0:3:1c:7f:15:4:5:1:0:11:13:1a:0:12:f:17:16:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0
