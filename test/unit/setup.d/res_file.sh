#!/bin/bash

task "Setting up files for res_file unit tests"

mkdir -p $TEST_UNIT_TEMP/res_file
mkdir -p $TEST_UNIT_DATA/res_file

F=$TEST_UNIT_TEMP/res_file/delete
cat > $F <<EOF
This file will be deleted during a successfull test run.
EOF

F=$TEST_UNIT_TEMP/res_file/fstab
cat > $F <<EOF
#
# /etc/fstab: file system information.
#
# <file system> <mount point>   <type>  <options>       <dump>  <pass>
proc            /proc           proc    defaults        0       0
LABEL=/         /               ext3    defaults        0       1
LABEL=swap      none            swap    sw              0       0
/dev/hda        /media/cdrom0   udf     user,noauto     0       0
EOF
chown 42:shadow $F
chmod 0640 $F

cp $F $TEST_UNIT_DATA/res_file

cat > $TEST_UNIT_TEMP/res_file/sudoers <<EOF
# /etc/sudoers
#
# This file MUST be edited with the 'visudo' command as root.
#
# See the man page for details on how to write a sudoers file.
#

Defaults        env_reset

# Uncomment to allow members of group sudo to not need a password
# %sudo ALL=NOPASSWD: ALL

# Host alias specification

# User alias specification

# Cmnd alias specification

# User privilege specification
root    ALL=(ALL) ALL

# Members of the admin group may gain root privileges
%admin ALL=(ALL) ALL
# QUIT BOTHERING ME!
jrhunt ALL=(ALL) NOPASSWD: ALL
EOF
