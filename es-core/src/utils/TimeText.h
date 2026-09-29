#pragma once
#ifndef ES_CORE_UTILS_TIME_TEXT_H
#define ES_CORE_UTILS_TIME_TEXT_H

#include <string>
#include <time.h>

// The time of day as the player reads it beside a date (fork #195, D-UI-058).
//
// Pure: a tm and what the caller already knows -- the switch, and what the
// locale says for %p -- so es-app/tests/unit can hold it to its word. The
// shell that asks Settings and strftime is DateTime::toLocalTimeString in
// TimeUtil.cpp; the SAVE STATE MANAGER's tiles are what this was written for.
namespace Utils
{
	namespace Time
	{
		// t:            as localtime fills it; tm_hour 0..23, tm_min 0..59.
		// twelveHour:   SHOW CLOCK IN 12-HOUR FORMAT, read at the call.
		// localeMarker: the locale's %p for this hour -- "AM"/"PM" in English,
		//               empty in French and most of Europe. On the 12-hour
		//               clock an empty marker gets a literal AM or PM from the
		//               hour instead, because "09:07" alone is exactly what
		//               the switch was turned on to avoid.
		// Off the switch: HH:MM, as %R prints it, marker or no marker.
		// On it: hh:MM and the marker, as %I:%M %p prints it -- the clock in
		// the corner and the cloud rows' LAST lines use the same form.
		std::string clockText(const tm& t, bool twelveHour, const std::string& localeMarker);

		// Which day a stamp falls on, as a player would say it (fork #195,
		// D-UI-087): TODAY reads as the time alone, YESTERDAY as that word
		// and the time, OLDER as the date and the time. Calendar days in
		// local time, so 23:59 and 00:01 across midnight are a day apart
		// and December 31st to January 1st is one day too. A stamp on a
		// later day than now is OLDER: the date is the honest thing to show
		// for a clock that was wrong when the file was written.
		enum class DayRelation { Today, Yesterday, Older };
		DayRelation dayRelation(const tm& stamp, const tm& now);

		// The locale's date with a two-digit year: "09/24/2026" -> "09/24/26",
		// "24/09/2026" -> "24/09/26". A save is not from another century, and
		// the two characters buy the word between the date and the time
		// (fork #195, D-UI-089, the maintainer's call). A date with no
		// four-digit year is returned as it came.
		std::string shortYear(const std::string& localeDate);

		// When a save was made, as the tile says it (D-UI-089): "TODAY at
		// 17:07", "YESTERDAY at 14:03", "09/24/26 at 14:03". The words come
		// translated from the caller -- TODAY, YESTERDAY in the label case,
		// "at" lower-case by the maintainer's word ("adding 'at', in
		// lowercase, if possible"), the a-grave in French.
		std::string whenText(DayRelation relation, const std::string& shortDate, const std::string& timeText,
		                     const std::string& todayWord, const std::string& yesterdayWord, const std::string& atWord);
	}
}

#endif // ES_CORE_UTILS_TIME_TEXT_H
