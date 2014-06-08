#!/bin/bash
for PN in $(find t/tmp/data/pn/fs -name '*.pn'); do
	./t/memcheck/verify ./pn --nofork $PN || exit $?
done
