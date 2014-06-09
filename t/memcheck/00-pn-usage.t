#!/bin/bash
SCRIPTPATH=$(cd $(dirname $0); pwd -P);
$SCRIPTPATH/verify ./pn -h
