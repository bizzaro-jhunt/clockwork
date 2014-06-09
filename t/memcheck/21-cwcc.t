#!/bin/bash
SCRIPTPATH=$(cd $(dirname $0); pwd -P);
$SCRIPTPATH/verify ./cwcc t/tmp/data/policy/manifest.pol
