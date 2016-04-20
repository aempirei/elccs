#!/bin/bash

if [ -z "$1" ] || [ ! -c "$1" ]; then
	echo
	echo usage: $0 arduino_device
	echo
	exit
fi

stty -F $1 406:0:18b2:8a30:3:1c:7f:8:4:2:64:0:11:13:1a:0:12:f:17:16:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0

stty -F $1
