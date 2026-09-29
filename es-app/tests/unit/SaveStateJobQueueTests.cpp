// The save state manager's job queue, checked without a device.
//
// Everything here runs against es-app/src/SaveStateJobQueue.cpp alone --
// no thread, no file, no script. The worker around it (SaveStateBookkeeper)
// is the part the VM tests; these hold the rules the worker relies on
// (fork #205 / #206, D-UI-073): one job at a time, a file pending deletion
// hides its tile while a copy hides nothing, nothing is queued for deletion
// twice, and the finished count is how a page learns.

#include "doctest/doctest.h"

#include "SaveStateJobQueue.h"

#include <string>

TEST_CASE("a fresh queue is idle and has finished nothing")
{
	SaveStateJobQueue q;
	CHECK(q.idle());
	CHECK(q.queued() == 0);
	CHECK_FALSE(q.running());
	CHECK(q.completed() == 0);
	CHECK_FALSE(q.isPending("/storage/roms/savestates/nes/Bobl.state1"));

	SaveStateJob job;
	CHECK_FALSE(q.take(job));
	q.finish();   // nothing running: nothing changes
	CHECK(q.completed() == 0);
}

TEST_CASE("a deletion makes the file pending at once, before any job runs")
{
	SaveStateJobQueue q;
	CHECK(q.enqueueDelete("/s/nes/Bobl.state4", "/s/nes/Bobl.state4.png"));
	CHECK(q.isPending("/s/nes/Bobl.state4"));
	CHECK(q.queued() == 1);
	CHECK_FALSE(q.idle());
	CHECK_FALSE(q.running());
	// The thumbnail is carried with the job; it is not a key of its own.
	CHECK_FALSE(q.isPending("/s/nes/Bobl.state4.png"));
}

TEST_CASE("a copy to be recorded hides nothing: its tile is real already")
{
	SaveStateJobQueue q;
	CHECK(q.enqueueCopy("/s/nes/Bobl.state4", "/s/nes/Bobl.state3", "nes", "/storage/roms/nes/Bobl.nes"));
	CHECK_FALSE(q.isPending("/s/nes/Bobl.state4"));
	CHECK_FALSE(q.isPending("/s/nes/Bobl.state3"));
	CHECK(q.queued() == 1);

	SaveStateJob job;
	REQUIRE(q.take(job));
	CHECK(job.kind == SaveStateJob::Kind::Copy);
	CHECK(job.stateFile == "/s/nes/Bobl.state4");
	CHECK(job.source == "/s/nes/Bobl.state3");
	CHECK(job.system == "nes");
	CHECK(job.rom == "/storage/roms/nes/Bobl.nes");
	q.finish();
	CHECK(q.completed() == 1);
}

TEST_CASE("a copy with no source or no destination is refused")
{
	SaveStateJobQueue q;
	CHECK_FALSE(q.enqueueCopy("", "/s/nes/Bobl.state3", "nes", "/r"));
	CHECK_FALSE(q.enqueueCopy("/s/nes/Bobl.state4", "", "nes", "/r"));
	CHECK(q.idle());
}

TEST_CASE("a file is queued for deletion once while pending, and again after it has gone")
{
	SaveStateJobQueue q;
	CHECK(q.enqueueDelete("/s/nes/Bobl.state4", ""));
	CHECK_FALSE(q.enqueueDelete("/s/nes/Bobl.state4", ""));
	CHECK(q.queued() == 1);

	SaveStateJob job;
	REQUIRE(q.take(job));
	CHECK(job.kind == SaveStateJob::Kind::Delete);
	CHECK_FALSE(q.enqueueDelete("/s/nes/Bobl.state4", ""));   // running counts as pending
	q.finish();
	CHECK_FALSE(q.isPending("/s/nes/Bobl.state4"));
	CHECK(q.enqueueDelete("/s/nes/Bobl.state4", ""));         // gone, so it can go again
}

TEST_CASE("an empty state path is refused for deletion")
{
	SaveStateJobQueue q;
	CHECK_FALSE(q.enqueueDelete("", "/s/nes/Bobl.state4.png"));
	CHECK(q.idle());
	CHECK_FALSE(q.isPending(""));
}

TEST_CASE("one job at a time: take hands out nothing while a job runs")
{
	SaveStateJobQueue q;
	q.enqueueDelete("/s/nes/Bobl.state1", "/s/nes/Bobl.state1.png");
	q.enqueueDelete("/s/nes/Bobl.state2", "");

	SaveStateJob first, second;
	REQUIRE(q.take(first));
	CHECK(first.stateFile == "/s/nes/Bobl.state1");
	CHECK(first.screenshot == "/s/nes/Bobl.state1.png");
	CHECK(q.running());
	CHECK(q.queued() == 1);

	CHECK_FALSE(q.take(second));   // the second waits for finish()
	CHECK(second.stateFile.empty());

	// Both stay pending: the first is running, the second is queued.
	CHECK(q.isPending("/s/nes/Bobl.state1"));
	CHECK(q.isPending("/s/nes/Bobl.state2"));

	q.finish();
	CHECK(q.completed() == 1);
	CHECK_FALSE(q.isPending("/s/nes/Bobl.state1"));
	CHECK(q.isPending("/s/nes/Bobl.state2"));

	REQUIRE(q.take(second));
	CHECK(second.stateFile == "/s/nes/Bobl.state2");
	CHECK(second.screenshot.empty());
	q.finish();
	CHECK(q.completed() == 2);
	CHECK(q.idle());
}

TEST_CASE("a copy and a deletion of the same file run in the order asked: the record, then the retire")
{
	SaveStateJobQueue q;
	q.enqueueCopy("/s/nes/Bobl.state4", "/s/nes/Bobl.state3", "nes", "/r/Bobl.nes");
	q.enqueueDelete("/s/nes/Bobl.state4", "/s/nes/Bobl.state4.png");
	CHECK(q.isPending("/s/nes/Bobl.state4"));   // the deletion is queued behind the copy

	SaveStateJob job;
	REQUIRE(q.take(job)); CHECK(job.kind == SaveStateJob::Kind::Copy); q.finish();
	CHECK(q.isPending("/s/nes/Bobl.state4"));   // still: the deletion has not run
	REQUIRE(q.take(job)); CHECK(job.kind == SaveStateJob::Kind::Delete); q.finish();
	CHECK_FALSE(q.isPending("/s/nes/Bobl.state4"));
	CHECK(q.completed() == 2);
}

TEST_CASE("jobs run in the order they were asked for")
{
	SaveStateJobQueue q;
	const std::string a = "/s/gbc/Tobu.state3", b = "/s/gbc/Tobu.state1", c = "/s/gbc/Tobu.state.auto";
	q.enqueueDelete(a, ""); q.enqueueDelete(b, ""); q.enqueueDelete(c, "");

	SaveStateJob job;
	REQUIRE(q.take(job)); CHECK(job.stateFile == a); q.finish();
	REQUIRE(q.take(job)); CHECK(job.stateFile == b); q.finish();
	REQUIRE(q.take(job)); CHECK(job.stateFile == c); q.finish();
	CHECK(q.completed() == 3);
	CHECK_FALSE(q.take(job));
}

TEST_CASE("completed moves once per finished job, which is what a page watches")
{
	SaveStateJobQueue q;
	unsigned seen = q.completed();
	q.enqueueDelete("/s/nes/Bobl.state4", "");
	CHECK(q.completed() == seen);   // queued is not finished

	SaveStateJob job;
	q.take(job);
	CHECK(q.completed() == seen);   // running is not finished either
	q.finish();
	CHECK(q.completed() == seen + 1);
	q.finish();                     // a stray finish with nothing running
	CHECK(q.completed() == seen + 1);
}
