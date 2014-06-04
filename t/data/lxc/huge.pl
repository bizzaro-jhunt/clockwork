#!/usr/bin/perl

my $factor = $ARGV[0] || 100;
my $USERS    =  4 * $factor;
my $GROUPS   =  1 * $factor;
my $PACKAGES =  4 * $factor;
my $SERVICES =  2 * $factor;
my $FILES    = 10 * $factor;
my $n;

print <<EOF;
# default policy, factor $factor
policy "default" {
EOF
for (1 .. $USERS) {
	my $id = $_ + 1000;
	$n++;
	print <<EOF
	user "user$_" {
		uid:  $id
		gid:  $id
		home: "/home/user$_"
		mkhome: "yes"
	}
EOF
}
print "\n";
for (1 .. $GROUPS) {
	my $id = $_ + 7000;
	$n++;
	print <<EOF
	group "group$_" {
		gid: $id
	}
EOF
}
print "\n";
for (1 .. $PACKAGES/2) {
	$n++;
	print <<EOF
	package "inst$_"   { version: "1.3.$_-1" }
EOF
}
print "\n";
for (1 .. $PACKAGES/2) {
	$n++;
	print <<EOF
	package "uninst$_" { installed: "no" }
EOF
}
for (1 .. $SERVICES/2) {
	$n++;
	print <<EOF
	service "run$_" {
		enabled: "yes"
		running: "yes"
	}
EOF
}
print "\n";
for (1 .. $SERVICES/2) {
	$n++;
	print <<EOF
	service "stop$_" {
		enabled: "no"
		running: "no"
	}
EOF
}
print "\n";
for (1 .. $FILES) {
	$n++;
	my $uid = ($_ > $USERS  ? "root" : "user$_");
	my $gid = ($_ > $GROUPS ? "root" : "group$_");
	print <<EOF
	file "/var/stuf/file.$_" {
		owner:  "$uid"
		group:  "$gid"
		mode:   0644
		source: "/cfm/files/var/stuf/file.$_"
	}
EOF
}
print <<EOF;

	# $n resources total
}
host fallback { enforce "default" }
EOF
