#pragma once
#ifndef ES_APP_TEXT_FIT_H
#define ES_APP_TEXT_FIT_H

// One line of text clipped to a width with an ellipsis: the long-job pages'
// fitting (GuiCloudTransfer, GuiOfflineScan), measured by the caller's font.
//
// Cut on characters, not bytes (#308 1-raoffline gpt F-RA-23,
// 5-cloud-sync-and-saves gpt F-CS-32). Both pages keep a name's UTF-8
// (es-code-traps.md: rclone shortens with U+2026, a ROM is named in any
// script), and the loop popped one byte at a time: a name ending in an
// accented or Japanese character was measured, and could be shown, with
// half a character on its end.
#include <string>

namespace TextFit
{
	// Drop the last character: its lead byte and every continuation byte.
	inline void popCharacter(std::string& text)
	{
		if (text.empty())
			return;
		size_t i = text.size() - 1;
		while (i > 0 && (static_cast<unsigned char>(text[i]) & 0xC0) == 0x80)
			i--;
		text.erase(i);
	}

	template <class Measure>
	std::string fitOneLine(std::string text, float width, Measure measure)
	{
		if (text.empty() || measure(text) <= width)
			return text;
		while (text.size() > 4 && measure(text + "...") > width)
			popCharacter(text);
		return text + "...";
	}
}

#endif // ES_APP_TEXT_FIT_H
