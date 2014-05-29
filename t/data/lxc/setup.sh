#!/bin/bash
export LC_ALL=C

if [[ ! -d /cw ]]; then
	echo >&2 "/cw not found; are you in an LXC environment?"
	exit 1
fi

RC=0

function group_should_not_exist() {
	local group=$1
	local fail=0
	for DB in "group" "gshadow"; do
		getent $DB $group >/dev/null 2>&1
		if [[ $? == 0 ]]; then
			echo >&2 "Found group '$group' in $DB database:"
			getent $DB $group >&2
			fail=2
		fi
	done
	if [[ $fail == 0 ]]; then
		echo >&2 "Group $group not found... OK"
	else
		RC=$fail
	fi
}

function user_should_not_exist() {
	local user=$1
	local fail=0
	for DB in "passwd" "shadow"; do
		getent $DB $user >/dev/null 2>&1
		if [[ $? == 0 ]]; then
			echo >&2 "Found user '$user' in $DB database:"
			getent $DB $user >&2
			fail=2
		fi
	done
	if [[ $fail == 0 ]]; then
		echo >&2 "User $user not found... OK"
	else
		RC=$fail
	fi
}

function file_should_not_exist() {
	local file=$1
	if [[ -e $file ]]; then
		echo >&2 "Found $file on the filesystem"
		RC=2
	else
		echo >&2 "File $file not found... OK"
	fi
}

function symlink_should_exist() {
	local file=$1
	if [[ ! -e $file ]]; then
		echo >&2 "Didn't find $file on the filesystem"
		RC=2
	elif [[ ! -h $file ]]; then
		echo >&2 "$file is not a symbolic link"
		RC=2
	else
		echo >&2 "Symlink $file found... OK"
	fi
}

function package_should_be_installed() {
	local pkg=$1
	/usr/bin/dpkg-query -W -f='${Version}' "$pkg" >/dev/null 2>&1
	if [[ $? != 0 ]]; then
		echo >&2 "Package $pkg is not installed"
		RC=2
	else
		echo >&2 "Package $pkg installed... OK"
	fi
}

function package_should_not_be_installed() {
	local pkg=$1
	/usr/bin/dpkg-query -W -f='${Version}' "$pkg" >/dev/null 2>&1
	if [[ $? == 0 ]]; then
		echo >&2 "Package $pkg is installed"
		RC=2
	else
		echo >&2 "Package $pkg not installed... OK"
	fi
}

#####################################

(
apt-get install -y libzmq3 libaugeas0
rm -f /etc/motd
ln -sf /etc/issue /etc/motd
) >/dev/null 2>&1

#####################################

group_should_not_exist editors
group_should_not_exist copyedit

user_should_not_exist rjoseph
user_should_not_exist lmiller
user_should_not_exist smoss

file_should_not_exist /home/rjoseph
file_should_not_exist /home/rjoseph/.ssh
file_should_not_exist /home/rjoseph/.vimrc
file_should_not_exist /home/rjoseph/env
file_should_not_exist /home/rjoseph/env/gitprompt
file_should_not_exist /home/rjoseph/env/git.bashrc

symlink_should_exist /etc/motd

package_should_not_be_installed ntp
package_should_be_installed ntpdate
