#!/bin/bash

/cw/sbin/clockd -c /cw/cfm/clockd.conf
/cw/sbin/cogd   -c /cw/cfm/cogd.conf
sleep 11
killall cogd
killall clockd

ps -ef | egrep 'cogd|clockd'
