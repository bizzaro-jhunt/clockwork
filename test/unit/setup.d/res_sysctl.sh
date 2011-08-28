#!/bin/bash

task "Setting up /proc for RES_SYSCTL unit tests"

echo 0 > /proc/sys/net/ipv4/conf/all/log_martians
