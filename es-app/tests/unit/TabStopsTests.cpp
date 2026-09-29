// A tabbed text's column stops (Font::getTabStops; #308 8b-es-core gpt
// F-ES-11), from the widths of what is in each column.

#include "doctest/doctest.h"

#include "resources/TabStops.h"

#include <map>
#include <vector>

TEST_CASE("one tab: the stop is the widest text before it")
{
	// "Achievements (hardcore):\t12" over "Points:\t340".
	const auto stops = TabStops::fromColumns({ { 24.0f }, { 7.0f } }, 1.0f);
	REQUIRE(stops.size() == 1);
	CHECK(stops.at(0) == doctest::Approx(24.0f));
}

TEST_CASE("a later column starts after the widest text of every column before it (#308 8b gpt F-ES-11)")
{
	// AAAAAAAAAA\tb\t1
	// c\tDDDDDDDDDD\t2
	// The second row's D column is drawn from stop 0 (the A column's width)
	// plus the gap, so it ends ten units past that; stop 1 was measured from
	// each line's own unaligned text (1 + 10, 10 + 1) and put the "2" inside
	// the D column.
	const float gap = 1.0f;
	const auto stops = TabStops::fromColumns({ { 10.0f, 1.0f }, { 1.0f, 10.0f } }, gap);
	REQUIRE(stops.size() == 2);
	CHECK(stops.at(0) == doctest::Approx(10.0f));
	CHECK(stops.at(1) == doctest::Approx(10.0f + gap + 10.0f));

	// Every column's text ends at or before the next stop.
	const std::vector<std::vector<float>> rows = { { 10.0f, 1.0f }, { 1.0f, 10.0f } };
	for (auto& row : rows)
	{
		const float columnOneStart = stops.at(0) + gap;
		CHECK(columnOneStart + row[1] <= stops.at(1) + 0.001f);
	}
}

TEST_CASE("a line with fewer tabs leaves the later stops to the lines that have them")
{
	const auto stops = TabStops::fromColumns({ { 3.0f }, { 5.0f, 4.0f, 2.0f } }, 0.5f);
	REQUIRE(stops.size() == 3);
	CHECK(stops.at(0) == doctest::Approx(5.0f));
	CHECK(stops.at(1) == doctest::Approx(5.0f + 0.5f + 4.0f));
	CHECK(stops.at(2) == doctest::Approx(5.0f + 0.5f + 4.0f + 0.5f + 2.0f));
	CHECK(TabStops::fromColumns({}, 1.0f).empty());
}
