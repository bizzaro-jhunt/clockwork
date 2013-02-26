#
# Regular cron jobs for the clockwork package
#
0 4	* * *	root	[ -x /usr/bin/clockwork_maintenance ] && /usr/bin/clockwork_maintenance
