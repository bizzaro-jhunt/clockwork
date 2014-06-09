#!/bin/bash
SCRIPTPATH=$(cd $(dirname $0); pwd -P);
find t -maxdepth 1 -type f -perm /u=x | sort | xargs -n1 -I@ $SCRIPTPATH/verify @
