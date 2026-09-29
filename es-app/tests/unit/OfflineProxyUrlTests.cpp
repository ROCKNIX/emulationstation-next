// Whether a URL is the offline RetroAchievements proxy's, checked without a
// device (fork #199).
//
// Runs against es-core/src/utils/OfflineProxyUrl.cpp, with
// OfflineAchievementsText for the one thing the two must agree on. The case
// that wrote it: the summary page's rows fetched their icons from the proxy
// with no store-only header, so a proxy that still believed it was online
// went to the web for every icon it did not hold. The header is sent by the
// host, so the host test is the whole rule, and a rule about a string is
// judged on the strings that almost match.

#include "doctest/doctest.h"

#include "utils/OfflineProxyUrl.h"
#include "OfflineAchievementsText.h"

#include <string>

using Utils::OfflineProxy::Base;
using Utils::OfflineProxy::StoreOnlyHeader;
using Utils::OfflineProxy::isProxyUrl;

TEST_CASE("the pages' requests and the images' host test are built on one address")
{
	CHECK(std::string(Base) == "http://127.0.0.1:8080");
	CHECK(isProxyUrl(OfflineAchievementsText::requestUrl("r=patch&g=1&u=qa")));
	CHECK(isProxyUrl(OfflineAchievementsText::badgeUrl("109361", true)));
	CHECK(isProxyUrl(OfflineAchievementsText::badgeUrl("109361", false)));
}

TEST_CASE("the store-only header is the line the proxy's patches 010 and 012 read")
{
	CHECK(std::string(StoreOnlyHeader) == "X-RA-Store-Only: 1");
}

TEST_CASE("a URL on the proxy: the base alone, a path, a query, any case")
{
	CHECK(isProxyUrl("http://127.0.0.1:8080"));
	CHECK(isProxyUrl("http://127.0.0.1:8080/"));
	CHECK(isProxyUrl("http://127.0.0.1:8080/Images/000102.png"));
	CHECK(isProxyUrl("http://127.0.0.1:8080/Badge/109361_lock.png"));
	CHECK(isProxyUrl("http://127.0.0.1:8080/UserPic/qa.png"));
	CHECK(isProxyUrl("http://127.0.0.1:8080/dorequest.php?r=patch&g=1"));
	CHECK(isProxyUrl("http://127.0.0.1:8080?x=1"));
	CHECK(isProxyUrl("http://127.0.0.1:8080#top"));
	CHECK(isProxyUrl("HTTP://127.0.0.1:8080/Images/1.png"));
}

TEST_CASE("not the proxy: the web's image hosts, another scheme, port or none, a host that begins like it, no scheme, nothing")
{
	CHECK_FALSE(isProxyUrl("https://media.retroachievements.org/Images/000102.png"));
	CHECK_FALSE(isProxyUrl("http://i.retroachievements.org/Images/000102.png"));
	CHECK_FALSE(isProxyUrl("https://retroachievements.org/UserPic/qa.png"));
	CHECK_FALSE(isProxyUrl("https://127.0.0.1:8080/Images/1.png"));
	CHECK_FALSE(isProxyUrl("http://127.0.0.1:80800/Images/1.png"));
	CHECK_FALSE(isProxyUrl("http://127.0.0.1:8080.example.com/Images/1.png"));
	CHECK_FALSE(isProxyUrl("http://127.0.0.1:8081/Images/1.png"));
	CHECK_FALSE(isProxyUrl("http://127.0.0.1/Images/1.png"));
	CHECK_FALSE(isProxyUrl("http://localhost:8080/Images/1.png"));
	CHECK_FALSE(isProxyUrl("127.0.0.1:8080/Images/1.png"));
	CHECK_FALSE(isProxyUrl("http://127.0.0.1:808"));
	CHECK_FALSE(isProxyUrl(""));
	CHECK_FALSE(isProxyUrl(":/cartridge.svg"));
	CHECK_FALSE(isProxyUrl("/storage/roms/nes/images/a.png"));
}
