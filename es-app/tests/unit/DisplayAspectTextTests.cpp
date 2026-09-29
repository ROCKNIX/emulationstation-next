#include "doctest/doctest.h"

#include "DisplayAspectText.h"

// The table and the file-name rule behind the save state manager's tiles
// and the SCREENSHOTS list (fork #243, D-UI-080).
TEST_CASE("a 4:3 console's picture is shown at 4:3, a square-pixel handheld's at the file's own")
{
	CHECK(DisplayAspectText::forSystem("nes") == doctest::Approx(4.0f / 3.0f));
	CHECK(DisplayAspectText::forSystem("snes") == doctest::Approx(4.0f / 3.0f));
	CHECK(DisplayAspectText::forSystem("megadrive") == doctest::Approx(4.0f / 3.0f));
	CHECK(DisplayAspectText::forSystem("psx") == doctest::Approx(4.0f / 3.0f));
	CHECK(DisplayAspectText::forSystem("gb") == 0.0f);
	CHECK(DisplayAspectText::forSystem("gbc") == 0.0f);
	CHECK(DisplayAspectText::forSystem("gba") == 0.0f);
	CHECK(DisplayAspectText::forSystem("psp") == 0.0f);
	CHECK(DisplayAspectText::forSystem("arcade") == 0.0f);
	CHECK(DisplayAspectText::forSystem("") == 0.0f);
	CHECK(DisplayAspectText::forSystem("no-such-system") == 0.0f);
}

TEST_CASE("RetroArch's screenshot name gives the content back, and nothing else does")
{
	CHECK(DisplayAspectText::screenshotContent("Bobl-260922-153012.png") == "Bobl");
	CHECK(DisplayAspectText::screenshotContent("Dr. Mario (Japan, USA) (Rev A)-260922-003107.png") == "Dr. Mario (Japan, USA) (Rev A)");
	CHECK(DisplayAspectText::screenshotContent("Tobu-Tobu-260922-153012.png") == "Tobu-Tobu");
	CHECK(DisplayAspectText::screenshotContent("Bobl-260922-153012") == "Bobl");
	// the screenshot RetroArch takes at an achievement unlock
	CHECK(DisplayAspectText::screenshotContent("mspacman-cheevo-225135.png") == "mspacman");
	CHECK(DisplayAspectText::screenshotContent("bublbobl-cheevo-380824.png") == "bublbobl");
	CHECK(DisplayAspectText::screenshotContent("Dr. Mario (Japan, USA) (Rev A)-cheevo-8937.png") == "Dr. Mario (Japan, USA) (Rev A)");
	CHECK(DisplayAspectText::screenshotContent("mspacman-cheevo-.png") == "");
	CHECK(DisplayAspectText::screenshotContent("mspacman-cheevo-x1.png") == "");
	CHECK(DisplayAspectText::screenshotContent("-cheevo-1.png") == "");
	// a save-state thumbnail, a manual name, a date with letters, too short
	CHECK(DisplayAspectText::screenshotContent("Bobl.state1.png") == "");
	CHECK(DisplayAspectText::screenshotContent("holiday.png") == "");
	CHECK(DisplayAspectText::screenshotContent("Bobl-26O922-153012.png") == "");
	CHECK(DisplayAspectText::screenshotContent("-260922-153012.png") == "");
	CHECK(DisplayAspectText::screenshotContent("") == "");
}

TEST_CASE("a dotted name without its extension keeps its dot (#308 8a gpt F-ES-21)")
{
	// The part after the last dot was taken for an extension whatever it
	// was, so an extensionless name with a dot in the content lost all but
	// the text before it.
	CHECK(DisplayAspectText::screenshotContent("Dr. Mario-260922-153012") == "Dr. Mario");
	CHECK(DisplayAspectText::screenshotContent("Dr. Mario (Japan, USA) (Rev A)-cheevo-8937") == "Dr. Mario (Japan, USA) (Rev A)");
	CHECK(DisplayAspectText::screenshotContent("St. John's Quest v1.2-260922-153012") == "St. John's Quest v1.2");
	// An image's own extension still goes, in either case.
	CHECK(DisplayAspectText::screenshotContent("Dr. Mario-260922-153012.PNG") == "Dr. Mario");
	CHECK(DisplayAspectText::screenshotContent("Dr. Mario-260922-153012.jpg") == "Dr. Mario");
}
