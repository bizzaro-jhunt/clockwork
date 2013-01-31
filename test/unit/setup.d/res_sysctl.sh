#!/bin/bash

if [[ $UID != 0 ]]; then
	task "Skipping setup of /proc for RES_SYSCTL unit tests; not root"

else
	task "Setting up /proc for RES_SYSCTL unit tests"
	echo 0 > /proc/sys/net/ipv4/conf/all/log_martians
fi
