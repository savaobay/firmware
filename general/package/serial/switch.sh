#!/bin/bash

if [ "$1" == "sta" ]; then
	echo "Switching to STA mode..."
	ifdown wlan0
	cp /etc/sta /etc/network/interfaces.d/wlan0
	ifup wlan0
	fw_setenv wlanmode sta

elif [ "$1" == "ap" ]; then
	echo "Switching to AP mode..."
	ifdown wlan0
	cp /etc/ap /etc/network/interfaces.d/wlan0
	ifup wlan0
	fw_setenv wlanmode ap

else
	mode = $(fw_printenv -n wlanmode)
	echo "Default $mode mode..."
	if [ "$mode" == "ap" ]; then
		cp /etc/ap /etc/network/interfaces.d/wlan0
	else
		cp /etc/sta /etc/network/interfaces.d/wlan0
	fi
fi
