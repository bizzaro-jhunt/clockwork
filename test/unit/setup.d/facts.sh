#!/bin/bash

task "Setting up data for facts tests"

mkdir -p $TEST_UNIT_DATA/facts
mkdir -p $TEST_UNIT_TEMP/facts

F=$TEST_UNIT_DATA/facts/good.facts
cat > $F <<EOF
test.fact1=fact1
test.fact2=fact2
test.multi.level.fact=multilevel fact
EOF
