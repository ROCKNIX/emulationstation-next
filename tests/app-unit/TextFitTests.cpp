// The long-job pages' one-line fitting cuts on characters (#308 1-raoffline
// gpt F-RA-23, 5-cloud-sync-and-saves gpt F-CS-32): a game or file name in
// any script reaches the font, and the screen, whole or not at all.
#include "doctest/doctest.h"
#include "TextFit.h"

#include <string>
#include <vector>

namespace
{
	bool validUtf8(const std::string& s)
	{
		size_t i = 0;
		while (i < s.size())
		{
			const unsigned char c = (unsigned char) s[i];
			const int n = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 0;
			if (n == 0 || i + n > s.size())
				return false;
			for (int k = 1; k < n; k++)
				if (((unsigned char) s[i + k] & 0xC0) != 0x80)
					return false;
			i += n;
		}
		return true;
	}

	// A font of fixed advance: ten per character, as a font would see it.
	struct Font
	{
		std::vector<std::string> measured;
		float operator()(const std::string& s)
		{
			measured.push_back(s);
			float w = 0;
			for (unsigned char c : s)
				if ((c & 0xC0) != 0x80)
					w += 10;
			return w;
		}
	};
}

TEST_CASE("fit: a name in Japanese is cut on a character")
{
	Font font;
	std::string measured;
	const std::string name = "ドラゴンクエストV 天空の花嫁 (Japan)";
	const std::string out = TextFit::fitOneLine(name, 105.0f, [&font](const std::string& s) { return font(s); });
	INFO(out);
	CHECK(validUtf8(out));
	for (auto& s : font.measured)
		CHECK_MESSAGE(validUtf8(s), "the font was handed a cut character");
	CHECK(out.size() < name.size());
	CHECK(out.substr(out.size() - 3) == "...");
}

TEST_CASE("fit: an accented name keeps its accents whole")
{
	Font font;
	const std::string name = "Pokémon Édition Émeraude";
	const std::string out = TextFit::fitOneLine(name, 95.0f, [&font](const std::string& s) { return font(s); });
	INFO(out);
	CHECK(validUtf8(out));
	for (auto& s : font.measured)
		CHECK(validUtf8(s));
}

TEST_CASE("fit: what fits is left alone, and ASCII is cut as before")
{
	Font font;
	CHECK(TextFit::fitOneLine("SNES", 100.0f, [&font](const std::string& s) { return font(s); }) == "SNES");
	CHECK(TextFit::fitOneLine("ABCDEFGHIJKL", 90.0f, [&font](const std::string& s) { return font(s); }) == "ABCDEF...");
}
