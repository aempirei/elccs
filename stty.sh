#!/bin/bash

if [ "$1" == "-h" ] || [ "$1" == "--help" ]; then
	prog=`basename "$0"`
	echo
	echo "usage: $prog [OPTION]... [DEVICE]"
	echo
	echo "    -h, --help    show this help"
	echo
	exit 
fi

if [ -z "$1" ]; then
	device="/dev/ttyUSB?"
else
	device="$1"
fi

if [ -c $device ]; then
	echo configuring device $device
else
	echo ERROR: $device is not a valid character device.
	exit
fi

config='406:0:18b2:8a30:3:1c:7f:8:4:2:64:0:11:13:1a:0:12:f:17:16:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0'

stty -F $device "$config"
stty -F $device
