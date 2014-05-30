#!/bin/bash

if [[ ! -d /cw ]]; then
	echo >&2 "/cw not found; are you in an LXC environment?"
	exit 1
fi

RC=0

function group_should_exist() {
	local group=$1
	for DB in "group" "gshadow"; do
		getent $DB $group >/dev/null 2>&1
		if [[ $? != 0 ]]; then
			echo >&2 "Group '$group' not found in $DB database:"
			RC=2
		fi
	done
}

function user_should_exist() {
	local user=$1
	for DB in "passwd" "shadow"; do
		getent $DB $user >/dev/null 2>&1
		if [[ $? != 0 ]]; then
			echo >&2 "User '$user' not found in $DB database:"
			RC=2
		fi
	done
}

function file_should_exist() {
	local file=$1
	if [[ ! -e $file ]]; then
		echo >&2 "File $file not found on the filesystem"
		RC=2
	elif [[ ! -f $file ]]; then
		echo >&2 "File $file is not a regular file"
		RC=2
	fi
}

function file_sha1_should_be() {
	local file=$1
	local sha1=$2
	if [[ -f $file ]]; then
		local got=$(sha1sum $file | awk '{print $1}')
		if [[ $got != $sha1 ]]; then
			echo >&2 "SHA1 of $file ($got) != expected ($sha1)"
			RC=2
		fi
	fi
}

function dir_should_exist() {
	local dir=$1
	if [[ ! -e $dir ]]; then
		echo >&2 "Direcotry $dir not found on the filesystem"
		RC=2
	elif [[ ! -d $dir ]]; then
		echo >&2 "File $dir is not a directory"
		RC=2
	fi
}

function package_should_be_installed() {
	local pkg=$1
	/usr/bin/dpkg-query -W -f='${Version}' "$pkg" &>/dev/null
	if [[ $? != 0 ]]; then
		echo >&2 "Package $pkg is not installed"
		RC=2
	fi
}

function package_should_not_be_installed() {
	local pkg=$1
	/usr/bin/dpkg-query -W -f='${Version}' "$pkg" &>/dev/null
	if [[ $? == 0 ]]; then
		echo >&2 "Package $pkg is installed"
		RC=2
	fi
}

group_should_exist editors
group_should_exist copyedit

user_should_exist rjoseph
user_should_exist lmiller
user_should_exist smoss

dir_should_exist /home/rjoseph
dir_should_exist /home/rjoseph/.ssh
file_should_exist /home/rjoseph/.vimrc
file_sha1_should_be /home/rjoseph/.vimrc 45612457ba26f94ad404c72b90220856668d5d55
dir_should_exist /home/rjoseph/env
file_should_exist /home/rjoseph/env/gitprompt
file_sha1_should_be /home/rjoseph/env/gitprompt 036955bcc7013256f4c30e5acc60e10fa221daa1
file_should_exist /home/rjoseph/env/git.bashrc
file_sha1_should_be /home/rjoseph/env/git.bashrc e2def2b11b9ed7fbffcb297c99ccfc6f8046b6ca

file_should_exist /etc/motd
file_sha1_should_be /etc/motd 2920d4dc6ca787b79e214baeff72e1a4d3d71067

package_should_be_installed     ntp
package_should_not_be_installed ntpdate
