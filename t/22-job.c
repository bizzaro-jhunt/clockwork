/*
  Copyright 2011-2014 James Hunt <james@jameshunt.us>

  This file is part of Clockwork.

  Clockwork is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Clockwork is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Clockwork.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "test.h"

static void fake_job_run(struct job *job)
{
	job_start(job);
	usleep(1 * 1000000 + 123456);
	job_end(job);
}

static struct job* fake_job(void)
{
	struct job *job;
	struct report *report;

	job = job_new();

	report = report_new("User", "testuser1");
	job_add_report(job, report);
	report_action(report, string("creating home directory"), ACTION_SUCCEEDED);
	report_action(report, string("setting password"),        ACTION_FAILED);

	report = report_new("File", "/etc/sudoers");
	job_add_report(job, report);
	report_action(report, string("creating file"), ACTION_SUCCEEDED);

	report = report_new("Group", "testg1");
	job_add_report(job, report);
	report_action(report, string("Removing group"), ACTION_SUCCEEDED);

	return job;
}

TESTS {
	subtest {
		job_free(NULL);    pass("job_free(NULL) does not segfault");
		report_free(NULL); pass("report_free(NULL) does not segfault");
		action_free(NULL); pass("action_free(NULL) does not segfault");
	}

	subtest {
		struct job *job;

		isnt_null(job = job_new(), "created new job");
		is_int(job->duration,      0, "initial job duration");
		is_int(job->start.tv_sec,  0, "initial job start time (seconds)");
		is_int(job->start.tv_usec, 0, "initial job start time (microseconds)");
		is_int(job->end.tv_sec,    0, "initial job end time (seconds)");
		is_int(job->end.tv_usec,   0, "initial job end time (microseconds)");

		job_free(job);
	}

	subtest {
		struct job *job;

		isnt_null(job = job_new(), "created new job");
		fake_job_run(job);

		ok(job->start.tv_sec  > 0, "job has non-zero start time (seconds)");
		ok(job->start.tv_usec > 0, "job has non-zero start time (microseconds)");
		ok(job->end.tv_sec    > 0, "job has non-zero end time (seconds)");
		ok(job->end.tv_usec   > 0, "job has non-zero end time (microseconds)");
		ok(job->end.tv_sec >= job->start.tv_sec, "job ends after it starts");
		ok(job->duration > 1000000, "job ran form more than one second");

		job_free(job);
	}

	subtest {
		struct job *job = NULL;
		struct job *unpacked = NULL;
		char *packed, *repacked;

		isnt_null(job = fake_job(), "created fake job");
		fake_job_run(job);

		isnt_null(packed = job_pack(job), "packed job");
		isnt_null(unpacked = job_unpack(packed), "unpacked job");

		is_int(unpacked->start.tv_usec, 0, "unpacked job start time (ms)");
		is_int(unpacked->end.tv_usec,   0, "unpacked job end time (ms)");

		is_int(unpacked->start.tv_sec, job->start.tv_sec, "job start times (seconds) match");
		is_int(unpacked->end.tv_sec,   job->end.tv_sec,   "job end times (seconds) match");
		is_int(unpacked->duration,     job->duration,     "job durations match");

		repacked = job_pack(unpacked);
		is_string(repacked, packed, "pack(unpack(pack(job))) == pack(job)");

		free(packed);
		free(repacked);
		job_free(job);
		job_free(unpacked);
	}

	done_testing();
}
