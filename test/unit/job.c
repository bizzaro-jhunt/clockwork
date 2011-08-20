#include "test.h"
#include "assertions.h"

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
	assert_not_null("fake_job created a valid job", job);

	report = report_new("User", "testuser1");
	assert_not_null("fake_job created testuser1 report", report);
	job_add_report(job, report);
	report_action(report, string("creating home directory"), ACTION_SUCCEEDED);
	report_action(report, string("setting password"),        ACTION_FAILED);

	report = report_new("File", "/etc/sudoers");
	assert_not_null("fake_job created /etc/sudoers report", report);
	job_add_report(job, report);

	report = report_new("Group", "testg1");
	assert_not_null("fake_job created testg1 report", report);
	job_add_report(job, report);
	report_action(report, string("Removing group"), ACTION_SUCCEEDED);

	return job;
}

static void assert_jobs_eq(struct job *actual, struct job *expected)
{
	assert_not_null("actual job pointer is not null", actual);
	assert_not_null("expected job pointer is not null", expected);

	assert_int_eq("job start times (tv_sec) match", actual->start.tv_sec, expected->start.tv_sec);
	assert_int_eq("job end times (tv_sec) match", actual->end.tv_sec, expected->end.tv_sec);

	assert_int_eq("job durations match", actual->duration, expected->duration);
}

void test_job_creation()
{
	struct job *job = NULL;

	test("job: Job Creation");
	job = job_new();
	assert_not_null("job_new returns a valid pointer", job);
	assert_int_eq("initial duration is 0", job->duration, 0);
	assert_int_eq("initial start.tv_sec is 0", job->start.tv_sec, 0);
	assert_int_eq("initial start.tv_usec is 0", job->start.tv_usec, 0);
	assert_int_eq("initial end.tv_sec is 0", job->end.tv_sec, 0);
	assert_int_eq("initial end.tv_usec is 0", job->end.tv_usec, 0);

	job_free(job);
}

void test_job_timer()
{
	struct job *job = NULL;

	test("job: Job Timer");
	job = job_new();
	assert_not_null("job_new returns a valid pointer", job);

	fake_job_run(job);

	assert_int_ne("Job has non-zero start time (seconds)", job->start.tv_sec, 0);
	assert_int_ne("Job has non-zero start time (microseconds)", job->start.tv_usec, 0);
	assert_int_ne("Job has non-zero end time (seconds)", job->end.tv_sec, 0);
	assert_int_ne("Job has non-zero end time (microseconds)", job->end.tv_usec, 0);
	assert_int_ne("Job has non-zero duration", job->duration, 0);
	assert_int_ge("End time (seconds) is greater than or equal to start time", job->end.tv_sec, job->start.tv_sec);

	/* duration should be over 1 second, thanks to usleep(3) call */
	assert_int_gt("Job 'ran' for more than 1 second", job->duration, 1000000);

	job_free(job);
}

void test_job_pack_unpack()
{
	struct job *job = NULL;
	struct job *unpacked = NULL;
	char *packed, *repacked;

	test("job: Packing Job Structure (and reports / actions)");
	job = fake_job();
	fake_job_run(job);

	packed = job_pack(job);
	unpacked = job_unpack(packed);

	assert_int_eq("unpacked job start time (tv_usec) is 0", unpacked->start.tv_usec, 0);
	assert_int_eq("unpacked job end time (tv_usec) is 0", unpacked->end.tv_usec, 0);

	assert_jobs_eq(job, unpacked);

	repacked = job_pack(unpacked);
	assert_str_eq("Repacked string equals original packed string", repacked, packed);

	free(packed);
	free(repacked);
	job_free(job);
	job_free(unpacked);
}

void test_job_free_NULL()
{
	test("job: data structure frees with NULL args");
	job_free(NULL);    assert_true("job_free(NULL) does not segfault", 1);
	report_free(NULL); assert_true("report_free(NULL) does not segfault", 1);
	action_free(NULL); assert_true("action_free(NULL) does not segfault", 1);
}

void test_suite_job()
{
	test_job_creation();
	test_job_timer();
	test_job_pack_unpack();

	test_job_free_NULL();
}
