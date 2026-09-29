#pragma once
#ifndef ES_CORE_RESOURCES_TAB_STOPS_H
#define ES_CORE_RESOURCES_TAB_STOPS_H

#include <map>
#include <vector>

// Where a tabbed text's columns start, from what is in them (Font's
// getTabStops, sizeTabbedText and buildTextCache): pure, so es-unit-tests
// reaches it without a font (es-app/tests/unit/TabStopsTests.cpp).
namespace TabStops
{
	// `lines` holds, for each line, the width of each piece before a tab --
	// the piece after a line's last tab is no column's and is not listed.
	// The answer is one stop per tab index, in pixels from the line's start:
	// text after tab k is drawn at stop k plus `gap`.
	//
	// Column by column (#308 F-ES-11): stop 0 is the widest first column,
	// and each later stop is the one before it plus the gap plus the widest
	// text in its own column -- because that is where the drawing puts the
	// column, at the previous stop plus the gap. The stops used to be the
	// widest run of text before each tab measured along each line from its
	// own start, which is right for one tab and wrong for two: a short first
	// column beside a long second one gave a second stop that fell inside
	// another line's second column, and its third column was drawn over it.
	inline std::map<int, float> fromColumns(const std::vector<std::vector<float>>& lines, float gap)
	{
		std::vector<float> widest;
		for (auto& pieces : lines)
		{
			if (pieces.size() > widest.size())
				widest.resize(pieces.size(), 0.0f);
			for (size_t k = 0; k < pieces.size(); k++)
				if (pieces[k] > widest[k])
					widest[k] = pieces[k];
		}

		std::map<int, float> stops;
		float x = 0.0f;
		for (size_t k = 0; k < widest.size(); k++)
		{
			x += (k == 0 ? 0.0f : gap) + widest[k];
			stops[(int) k] = x;
		}
		return stops;
	}
}

#endif // ES_CORE_RESOURCES_TAB_STOPS_H
