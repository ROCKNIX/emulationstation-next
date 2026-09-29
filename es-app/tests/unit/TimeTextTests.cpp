// The time half of a save state's date, checked without a device (fork
// #195, D-UI-058).
//
// Runs against es-core/src/utils/TimeText.cpp alone. The case that wrote
// it: the AUTO SAVE row on the RG SP read "09/13/2026 09:07" for a Sunday
// morning file, and on a Monday night that was read as 9:07 pm. What
// matters here is that the 12-hour clock always says which half of the day
// it is, in a locale with a marker and in one without.

#include "doctest/doctest.h"

#include "utils/TimeText.h"

using Utils::Time::clockText;
using Utils::Time::dayRelation;
using Utils::Time::DayRelation;
using Utils::Time::shortYear;
using Utils::Time::whenText;

static tm at(int hour, int minute)
{
	tm t = {};
	t.tm_hour = hour;
	t.tm_min = minute;
	return t;
}

TEST_CASE("the switch off is the 24-hour clock, whatever the locale says for %p")
{
	CHECK(clockText(at(9, 7), false, "AM") == "09:07");
	CHECK(clockText(at(21, 7), false, "PM") == "21:07");
	CHECK(clockText(at(9, 7), false, "") == "09:07");
	CHECK(clockText(at(0, 5), false, "") == "00:05");
	CHECK(clockText(at(23, 59), false, "") == "23:59");
}

TEST_CASE("the switch on is %I:%M %p, the locale's marker after the hour")
{
	CHECK(clockText(at(9, 7), true, "AM") == "09:07 AM");
	CHECK(clockText(at(21, 7), true, "PM") == "09:07 PM");
	// Midnight and noon are 12, as %I writes them.
	CHECK(clockText(at(0, 5), true, "AM") == "12:05 AM");
	CHECK(clockText(at(12, 0), true, "PM") == "12:00 PM");
	CHECK(clockText(at(23, 59), true, "PM") == "11:59 PM");
}

TEST_CASE("a locale with no %p still tells morning from evening on the 12-hour clock")
{
	// French: strftime's %p is empty, so the hour alone would read "09:07"
	// twice a day -- the ambiguity the switch exists to remove.
	CHECK(clockText(at(9, 7), true, "") == "09:07 AM");
	CHECK(clockText(at(21, 7), true, "") == "09:07 PM");
	CHECK(clockText(at(0, 0), true, "") == "12:00 AM");
	CHECK(clockText(at(11, 59), true, "") == "11:59 AM");
	CHECK(clockText(at(12, 0), true, "") == "12:00 PM");
	CHECK(clockText(at(13, 0), true, "") == "01:00 PM");
}

TEST_CASE("the locale's own marker is used as it is")
{
	// A locale whose marker is not the English pair is not corrected.
	CHECK(clockText(at(9, 7), true, "a.m.") == "09:07 a.m.");
	CHECK(clockText(at(15, 30), true, "nachm.") == "03:30 nachm.");
}

TEST_CASE("the Sunday-morning file and the Monday-night reading are never the same text")
{
	// The case that filed #195: 09:07 on one half of the day read as the
	// other. With the switch on, no hour shares its text with the hour
	// twelve away from it, marker or no marker.
	for (int h = 0; h < 12; h++)
	{
		CHECK(clockText(at(h, 7), true, "") != clockText(at(h + 12, 7), true, ""));
		CHECK(clockText(at(h, 7), true, "AM") != clockText(at(h + 12, 7), true, "PM"));
	}
}

TEST_CASE("every hour writes the same number of characters, so one sample measures the widest tile")
{
	// GuiSaveState sizes the tile label from now() and trusts that text to
	// be as long as any row's. True only if every hour writes the same
	// count, marker included -- on both clocks.
	const size_t twelve = clockText(at(12, 0), true, "").size();
	const size_t twentyFour = clockText(at(12, 0), false, "").size();
	for (int h = 0; h < 24; h++)
	{
		CHECK(clockText(at(h, 0), true, "").size() == twelve);
		CHECK(clockText(at(h, 0), true, h < 12 ? "AM" : "PM").size() == twelve);
		CHECK(clockText(at(h, 0), false, "").size() == twentyFour);
	}
}

// The day rule behind the SAVE STATE MANAGER's tiles (fork #195, D-UI-087):
// today is the time alone, yesterday is the word and the time, anything
// else is the date. Calendar days, local time -- the player's midnight.
static tm on(int year, int month, int day, int hour = 12, int minute = 0)
{
	tm t = {};
	t.tm_year = year - 1900;
	t.tm_mon = month - 1;
	t.tm_mday = day;
	t.tm_hour = hour;
	t.tm_min = minute;
	return t;
}

TEST_CASE("the same calendar day is today, whatever the hours")
{
	CHECK(dayRelation(on(2026, 9, 24, 0, 1), on(2026, 9, 24, 23, 59)) == DayRelation::Today);
	CHECK(dayRelation(on(2026, 9, 24, 21, 7), on(2026, 9, 24, 21, 19)) == DayRelation::Today);
}

TEST_CASE("the day before is yesterday, across midnight, a month end and a year end")
{
	CHECK(dayRelation(on(2026, 9, 23, 23, 59), on(2026, 9, 24, 0, 1)) == DayRelation::Yesterday);
	CHECK(dayRelation(on(2026, 9, 30), on(2026, 10, 1)) == DayRelation::Yesterday);
	CHECK(dayRelation(on(2025, 12, 31), on(2026, 1, 1)) == DayRelation::Yesterday);
	CHECK(dayRelation(on(2028, 2, 29), on(2028, 3, 1)) == DayRelation::Yesterday);
	CHECK(dayRelation(on(2027, 2, 28), on(2027, 3, 1)) == DayRelation::Yesterday);
}

TEST_CASE("two days ago is older, and so is a stamp from the future")
{
	CHECK(dayRelation(on(2026, 9, 22, 23, 59), on(2026, 9, 24, 0, 1)) == DayRelation::Older);
	CHECK(dayRelation(on(2026, 3, 1), on(2026, 9, 24)) == DayRelation::Older);
	CHECK(dayRelation(on(2026, 9, 25, 9, 0), on(2026, 9, 24, 21, 0)) == DayRelation::Older);
	CHECK(dayRelation(on(2025, 9, 24), on(2026, 9, 24)) == DayRelation::Older);
}

// The words on the tile (D-UI-089): the day word or the short date, then
// "at" as the caller spells it, then the time.
TEST_CASE("the year loses its century, whatever the locale's order")
{
	CHECK(shortYear("09/24/2026") == "09/24/26");
	CHECK(shortYear("24/09/2026") == "24/09/26");
	CHECK(shortYear("2026-09-24") == "26-09-24");
	CHECK(shortYear("24.09.2026") == "24.09.26");
	CHECK(shortYear("09/24/26") == "09/24/26");
	CHECK(shortYear("") == "");
}

TEST_CASE("the tile says TODAY at, YESTERDAY at, or the short date at")
{
	CHECK(whenText(DayRelation::Today, "09/24/26", "17:07", "TODAY", "YESTERDAY", "at") == "TODAY at 17:07");
	CHECK(whenText(DayRelation::Yesterday, "09/24/26", "02:03 PM", "TODAY", "YESTERDAY", "at") == "YESTERDAY at 02:03 PM");
	CHECK(whenText(DayRelation::Older, "09/22/26", "14:03", "TODAY", "YESTERDAY", "at") == "09/22/26 at 14:03");
	CHECK(whenText(DayRelation::Yesterday, "24/09/26", "14:03", "AUJOURD'HUI", "HIER", "\xc3\xa0") == "HIER \xc3\xa0 14:03");
}

