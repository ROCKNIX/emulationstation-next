#pragma once
#ifndef ES_APP_DISPLAY_ASPECT_TEXT_H
#define ES_APP_DISPLAY_ASPECT_TEXT_H

#include <string>

// The pure half of DisplayAspect (fork #243, D-UI-080): which systems
// show their picture at a width-to-height their pixels do not have, and
// how RetroArch names a screenshot. No window, no filesystem, so the unit
// tests cover it.
namespace DisplayAspectText
{
	// The width-to-height a system's picture is shown at, by the system's
	// theme folder (nes, snes, megadrive, ...). 0 for a system whose
	// pixels are square (Game Boy, GBA, PSP), whose picture varies by game
	// (arcade), or that this table does not know: the file's own
	// proportions then.
	float forSystem(const std::string& themeFolder);

	// RetroArch names a screenshot "<content>-YYMMDD-HHMMSS.png" (and a
	// save-state thumbnail "<content>.stateN.png", which is not this).
	// The content's name, or "" when the name is not of that shape.
	std::string screenshotContent(const std::string& fileName);
}

#endif // ES_APP_DISPLAY_ASPECT_TEXT_H
