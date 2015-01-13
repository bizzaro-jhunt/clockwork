#!/bin/sh

if [ -f Makefile ]; then
	make distclean
fi

./bootrap
export CFLAGS=-g
./configure --prefix /cw
make
make DESTDIR=_nightly install
cp -R t/lxc _nightly/cw/cfm
cd _nightly;
tar -czf ../nightly.tar.gz *
cd ..
rm -rf _nightly

if [ "x$EXPORT" != "x" ]; then
	[ $EXPORT -lt 1024 ] && EXPORT=9909
	WHERE=$(ip addr show | grep inet.*global | sed -e 's/.*inet //;s@/.*@@;' | head -n1)
	echo
	echo "Ready."
	echo
	echo "To load into a nightly LXC tester,"
	echo "run \`cd /; nc $WHERE $EXPORT | tar -xzv\`"
	echo "from inside the container environment."
	nc -l $EXPORT < nightly.tar.gz
fi
