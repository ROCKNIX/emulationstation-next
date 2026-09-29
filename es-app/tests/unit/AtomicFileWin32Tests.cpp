// AtomicFileUtil's _WIN32 branches, compiled on this host with _WIN32
// defined for that one file (es-file-tests-win32; audit of the fixes of
// #307, claude G-E1-06). No ROCKNIX image builds for Windows and PL-075's
// header says the guarantees are POSIX's; what it promises of the Windows
// branches -- readText's ok among them -- is held here, where it can run.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "utils/AtomicFileUtil.h"

#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include <stdlib.h>
#include <unistd.h>

namespace
{
	struct ScratchDir
	{
		std::string path;
		ScratchDir()
		{
			const char* base = getenv("TMPDIR");
			std::string tmpl = std::string(base != nullptr && *base != '\0' ? base : "/tmp") + "/es-file-tests-win32.XXXXXX";
			std::vector<char> buf(tmpl.begin(), tmpl.end());
			buf.push_back('\0');
			REQUIRE(mkdtemp(buf.data()) != nullptr);
			path = buf.data();
		}
		~ScratchDir()
		{
			const std::string cmd = "rm -rf '" + path + "'";
			if (std::system(cmd.c_str()) != 0) {}
		}
	};
}

TEST_CASE("the _WIN32 readText: an empty file is read, a missing one is not (claude G-E1-06)")
{
	// `ss << in.rdbuf()` sets failbit on ss when nothing was inserted, and
	// the branch read failbit as a failed read: an empty file came back not
	// read, against the header's "empty and missing both read \"\"; only
	// the first is ok".
	ScratchDir dir;
	const std::string empty = dir.path + "/empty";
	{ std::ofstream out(empty); }
	bool ok = false;
	CHECK(Utils::AtomicFile::readText(empty, &ok).empty());
	CHECK(ok);

	ok = true;
	CHECK(Utils::AtomicFile::readText(dir.path + "/missing", &ok).empty());
	CHECK_FALSE(ok);

	const std::string full = dir.path + "/full";
	{ std::ofstream out(full, std::ios::binary); out << "a=1\n"; }
	ok = false;
	CHECK(Utils::AtomicFile::readText(full, &ok) == "a=1\n");
	CHECK(ok);

	// And a read that fails is not a read (a directory opens here, then fails).
	ok = true;
	CHECK(Utils::AtomicFile::readText(dir.path, &ok).empty());
	CHECK_FALSE(ok);
}

TEST_CASE("the _WIN32 writeText replaces the file whole")
{
	ScratchDir dir;
	const std::string path = dir.path + "/system.cfg";
	CHECK(Utils::AtomicFile::writeText(path, "a=1\n"));
	CHECK(Utils::AtomicFile::writeText(path, "a=2\n"));
	bool ok = false;
	CHECK(Utils::AtomicFile::readText(path, &ok) == "a=2\n");
	CHECK(ok);
}
