#include "DisplayAspectText.h"

#include <cctype>
#include <set>

namespace DisplayAspectText
{
	float forSystem(const std::string& themeFolder)
	{
		// Consoles and computers whose cores emit non-square pixels and
		// whose picture is a 4:3 screen: the NES's 256x240 and the SNES's
		// 256x224 are both 4:3 on the screen they were made for; the Mega
		// Drive's 320x224 and 256x224 likewise; the PC Engine's 256-wide
		// modes; the 8-bit computers and consoles; the disc systems whose
		// software cores render at native size. Handhelds are absent on
		// purpose: their pixels are square and the file is right as it
		// is. Arcade is absent too: a vertical shooter is 3:4.
		static const std::set<std::string> fourByThree = {
			"nes", "famicom", "fds", "nesh",
			"snes", "sfc", "snesh", "snesmsu1", "satellaview", "sufami",
			"megadrive", "genesis", "megadriveh", "genh", "megadrive-japan", "sega32x", "segacd", "megacd",
			"mastersystem", "sg-1000",
			"pcengine", "tg16", "supergrafx", "pcenginecd", "tg16cd", "pcfx",
			"atari2600", "atari5200", "atari7800", "atari800", "atarist",
			"c64", "c128", "vic20", "c16", "pet", "amiga", "amigacd32",
			"msx", "msx2", "colecovision", "intellivision", "odyssey2", "videopac", "channelf",
			"amstradcpc", "zxspectrum", "zx81", "x1", "x68000", "pc-8800", "pc-9800",
			"psx", "saturn", "neogeo", "neocd", "cdi", "3do", "n64", "n64dd",
		};
		return fourByThree.count(themeFolder) ? 4.0f / 3.0f : 0.0f;
	}

	std::string screenshotContent(const std::string& fileName)
	{
		// Two shapes RetroArch writes: "<content>-YYMMDD-HHMMSS" for a
		// screenshot the player took (twelve digits in two groups of six
		// after the last two dashes), and "<content>-cheevo-<id>" for the
		// one it takes at an achievement unlock (fork #248: the
		// maintainer's SCREENSHOTS folder was mostly the second kind).
		// With or without the extension -- an image's: the part after the
		// last dot of an extensionless "Dr. Mario-260922-153012" is the
		// content's, and taking it for an extension left "Dr" (#308 8a gpt
		// F-ES-21).
		std::string stem = fileName;
		const size_t dot = stem.rfind('.');
		if (dot != std::string::npos && dot > 0)
		{
			std::string ext = stem.substr(dot + 1);
			for (auto& c : ext)
				c = (char) tolower((unsigned char) c);
			static const std::set<std::string> images = { "png", "jpg", "jpeg", "bmp", "gif", "webp" };
			if (images.count(ext))
				stem = stem.substr(0, dot);
		}
		const size_t cheevo = stem.rfind("-cheevo-");
		if (cheevo != std::string::npos && cheevo > 0 && cheevo + 8 < stem.size())
		{
			bool digits = true;
			for (size_t i = cheevo + 8; i < stem.size(); i++)
				if (!isdigit((unsigned char) stem[i]))
					digits = false;
			if (digits)
				return stem.substr(0, cheevo);
		}
		if (stem.size() < 15)
			return "";
		const std::string tail = stem.substr(stem.size() - 14);
		if (tail[0] != '-' || tail[7] != '-')
			return "";
		for (size_t i = 1; i < 14; i++)
			if (i != 7 && !isdigit((unsigned char) tail[i]))
				return "";
		const std::string content = stem.substr(0, stem.size() - 14);
		return content;
	}
}
