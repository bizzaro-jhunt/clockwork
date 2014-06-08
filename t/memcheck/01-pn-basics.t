#!/bin/bash
for PN in $(find t/tmp/data/pn/basics -name '*.pn'); do
	./t/memcheck/verify ./pn --nofork $PN || exit $?
done
