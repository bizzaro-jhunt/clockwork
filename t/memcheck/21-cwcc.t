#!/bin/bash
SCRIPTPATH=$(cd $(dirname $0); pwd -P);
$SCRIPTPATH/verify ./cw-cc t/tmp/data/policy/manifest.pol
