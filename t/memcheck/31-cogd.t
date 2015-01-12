#!/bin/bash
# vim:ft=sh

SCRIPTPATH=$(cd $(dirname $0); pwd -P);
ETC=t/tmp/memcheck
mkdir -p $ETC \
         $ETC/gather.d \
         $ETC/copydown/src \
         $ETC/copydown/dst

cat >$ETC/cogd.conf <<EOF
master.1        127.0.0.1:2315
cert.1          $ETC/cert.pub
mesh.control    127.0.0.1:2316
mesh.broadcast  127.0.0.1:2317
mesh.cert       $ETC/cert.pub
gatherers       $ETC/gather.d/*
copydown        $ETC/copydown/dst
security.cert   $ETC/cert
acl.default     allow
EOF

cat >$ETC/clockd.conf <<EOF
security.cert    $ETC/cert
security.strict  no
manifest         $ETC/manifest.pol
listen           *:2315
pidfile          $ETC/clockd.pid
copydown         $ETC/copydown/src
EOF

cat >$ETC/cert.pub <<EOF
id  test.memcheck
pub 6d63011ac9b7ca9519e115c45e1d562bc0ac42815899f77e14a430a414326f01
EOF

cat >$ETC/cert <<EOF
id  test.memcheck
pub 6d63011ac9b7ca9519e115c45e1d562bc0ac42815899f77e14a430a414326f01
sec 9e00e990db4c9ff972c632813b8ce5096df69d50e4422b1268a9aa89697a1af1
EOF

cat >$ETC/gather.d/core <<EOF
#!/bin/bash
echo "test.env=yes"
EOF
chmod 755 $ETC/gather.d/core

cat >$ETC/copydown/src/newfile <<EOF
this is a file from the master
EOF

cat >$ETC/copydown/dst/oldfile <<EOF
this is a file already present on the client
EOF

cat >$ETC/manifest.pol <<EOF
policy "default" {
	file "/tmp/xyzzy-clockwork-memcheck.test" { }
}
host fallback { enforce "default" }
EOF

###############################################################################

export COGD_SKIP_ROOT=1
./TEST_clockd -c $ETC/clockd.conf -F &
$SCRIPTPATH/verify ./TEST_cogd -c $ETC/cogd.conf -X || exit $?
