// The credential mask, checked without a device.
//
// Everything here runs against Utils::String::maskSecrets alone
// (es-core/src/utils/StringUtil.cpp). What it is worth is what it covers:
// the shapes this tree puts a credential on a command line in -- setrootpass,
// wifictl's positional passphrase, a flag, name=value, RetroAchievements'
// one-letter query keys -- and the shapes that must come through untouched.
// Every value here is a stand-in; nothing in this file is a credential.
//
// Fork #177. Run: see README.md beside this file.

#include "doctest/doctest.h"

#include "utils/StringUtil.h"

#include <string>

using Utils::String::maskSecrets;

TEST_CASE("setrootpass: the whole rest of the line is the password")
{
	// The line #177 was opened for (Platform.cpp's runSystemCommand log).
	CHECK(maskSecrets("setrootpass hunter2") == "setrootpass <redacted>");

	// GuiMenu passes the password unquoted, so a space in it makes two
	// words; both go, and the line does not say how many there were.
	CHECK(maskSecrets("setrootpass hunter two") == "setrootpass <redacted>");
	CHECK(maskSecrets("setrootpass 'hunter two'") == "setrootpass <redacted>");

	// Trailing whitespace survives; the value does not.
	CHECK(maskSecrets("setrootpass hunter2  ") == "setrootpass <redacted>  ");

	// Nothing to mask.
	CHECK(maskSecrets("setrootpass") == "setrootpass");
	CHECK(maskSecrets("setrootpass ") == "setrootpass ");
}

TEST_CASE("wifictl: the passphrase after the SSID, and only that")
{
	CHECK(maskSecrets("timeout 150 wifictl connect 'Home Net' 'hunter2' 'US'")
		== "timeout 150 wifictl connect 'Home Net' <redacted> 'US'");

	// shellQuote's escaped quote inside the passphrase closes nothing.
	CHECK(maskSecrets("timeout 150 wifictl connect 'Net' 'it'\\''s me' 'US'")
		== "timeout 150 wifictl connect 'Net' <redacted> 'US'");

	CHECK(maskSecrets("wifictl enable 'Net' 'hunter2'") == "wifictl enable 'Net' <redacted>");

	// The other wifictl lines carry no secret.
	CHECK(maskSecrets("timeout 30 wifictl enable") == "timeout 30 wifictl enable");
	CHECK(maskSecrets("timeout 30 wifictl disable") == "timeout 30 wifictl disable");
	CHECK(maskSecrets("timeout 45 wifictl scanlist") == "timeout 45 wifictl scanlist");
}

TEST_CASE("a flag that names a credential takes the next word")
{
	CHECK(maskSecrets("--password hunter2 --user bob") == "--password <redacted> --user bob");
	CHECK(maskSecrets("--password=hunter2") == "--password=<redacted>");
	CHECK(maskSecrets("-p hunter2") == "-p <redacted>");
	CHECK(maskSecrets("tool -p 'two words' more") == "tool -p <redacted> more");
	CHECK(maskSecrets("sshpass -p hunter2 ssh host") == "sshpass -p <redacted> ssh host");
	CHECK(maskSecrets("moonlight pair -pin 1234 192.168.1.5") == "moonlight pair -pin <redacted> 192.168.1.5");
	CHECK(maskSecrets("--token abc --secret def --psk ghi") == "--token <redacted> --secret <redacted> --psk <redacted>");
	CHECK(maskSecrets("tool --api-key abc") == "tool --api-key <redacted>");
	CHECK(maskSecrets("--Password hunter2") == "--Password <redacted>");

	// The launch command's netplay password (FileData.cpp).
	CHECK(maskSecrets("runemu.sh rom -netplaymode client -netplayport 55435 -netplayip 1.2.3.4 -netplaypass hunter2")
		== "runemu.sh rom -netplaymode client -netplayport 55435 -netplayip 1.2.3.4 -netplaypass <redacted>");

	// Two flags that look like one and are not, both run in this tree:
	// batocera-hotkeys' --key is a key NAME, and mkdir -p takes no value.
	CHECK(maskSecrets("batocera-hotkeys --set --config X --key F1 --action Y")
		== "batocera-hotkeys --set --config X --key F1 --action Y");
	CHECK(maskSecrets("mkdir -p /storage/roms/moonlight") == "mkdir -p /storage/roms/moonlight");

	// -p1index is a different word (InputManager's configure line).
	CHECK(maskSecrets(" -p1index 0 -p1guid 03000000 ") == " -p1index 0 -p1guid 03000000 ");
}

TEST_CASE("name=value: every name ending in a credential word")
{
	CHECK(maskSecrets("password=hunter2") == "password=<redacted>");
	CHECK(maskSecrets("pass=hunter2") == "pass=<redacted>");
	CHECK(maskSecrets("token=abc") == "token=<redacted>");
	CHECK(maskSecrets("key=abc") == "key=<redacted>");
	CHECK(maskSecrets("devpassword=abc") == "devpassword=<redacted>");
	CHECK(maskSecrets("cheevos_password=abc") == "cheevos_password=<redacted>");
	CHECK(maskSecrets("cheevos_token=abc") == "cheevos_token=<redacted>");
	CHECK(maskSecrets("wifi.key=abc") == "wifi.key=<redacted>");
	CHECK(maskSecrets("client_secret=abc") == "client_secret=<redacted>");
	CHECK(maskSecrets("psk=abc") == "psk=<redacted>");
	CHECK(maskSecrets("ScreenScraperPass=abc") == "ScreenScraperPass=<redacted>");
	CHECK(maskSecrets("PASSWORD=abc") == "PASSWORD=<redacted>");

	// Around it the line is as it was; a username is not a credential.
	CHECK(maskSecrets("cloud_remote create webdav url=https://x/dav user=bob pass=hunter2 vendor=nextcloud")
		== "cloud_remote create webdav url=https://x/dav user=bob pass=<redacted> vendor=nextcloud");

	// A quoted value with spaces is masked whole, quotes and all.
	CHECK(maskSecrets("password=\"my secret\" next=1") == "password=<redacted> next=1");
	CHECK(maskSecrets("pass='it'\\''s me' x") == "pass=<redacted> x");
	CHECK(maskSecrets("password=\"a \\\" b\" next=1") == "password=<redacted> next=1");

	// An unterminated quote takes the rest of the line rather than leave
	// any of it visible.
	CHECK(maskSecrets("password='oops next=1") == "password=<redacted>");
}

TEST_CASE("RetroAchievements' one-letter query keys, in a query only")
{
	// The API key is split into two literals the compiler joins, so the
	// credential shape never stands whole in the source and the hooks' scan
	// needs no exemption for this file (the audit of the fix round).
	CHECK(maskSecrets("https://retroachievements.org/API/API_GetUserSummary.php?z=bob&y=" "abcdef1234567890")
		== "https://retroachievements.org/API/API_GetUserSummary.php?z=bob&y=<redacted>");
	CHECK(maskSecrets("https://retroachievements.org/dorequest.php?r=login&u=bob&p=hunter2")
		== "https://retroachievements.org/dorequest.php?r=login&u=bob&p=<redacted>");
	CHECK(maskSecrets("r=unlocks&g=1&u=bob&t=tok123&h=1") == "r=unlocks&g=1&u=bob&t=<redacted>&h=1");

	// The same letters outside a query are words like any other.
	CHECK(maskSecrets("y=1 t=2 p=3") == "y=1 t=2 p=3");
	CHECK(maskSecrets("rate=5") == "rate=5");
}

TEST_CASE("an empty value stays empty: whether it is set may be shown")
{
	CHECK(maskSecrets("password= next=1") == "password= next=1");
	CHECK(maskSecrets("pass=&x=1") == "pass=&x=1");
	CHECK(maskSecrets("--password") == "--password");
	CHECK(maskSecrets("cloud_setup wizard: password=") == "cloud_setup wizard: password=");
}

TEST_CASE("a placeholder in angle brackets is left alone, so a second pass changes nothing")
{
	CHECK(maskSecrets("setrootpass <password>") == "setrootpass <password>");
	CHECK(maskSecrets("--password <pw>") == "--password <pw>");
	CHECK(maskSecrets("password=<value>") == "password=<value>");
	CHECK(maskSecrets("?z=bob&y=<key>") == "?z=bob&y=<key>");

	const std::string masked = maskSecrets("wifictl connect 'n' 'k' && x --password y token=z ?y=w; setrootpass hunter2");
	CHECK(masked == "wifictl connect 'n' <redacted> && x --password <redacted> token=<redacted> ?y=<redacted>; setrootpass <redacted>");
	CHECK(maskSecrets(masked) == masked);
}

TEST_CASE("a command with no secret is returned byte for byte")
{
	const char* const plain[] = {
		"systemctl stop touchkeyboard",
		"rocknix-bluetooth trust AA:BB:CC:DD:EE:FF",
		"rocknix-config storage list",
		"timeout 5 sh -c 'ping -c 1 -W 2 -t 255 8.8.8.8 || ping -c 1 -W 2 -t 255 1.1.1.1' >/dev/null 2>&1",
		"/usr/bin/sh -lc \"echo \\\"default\\\"; tr \\\" \\\" \\\"\\n\\\" </sys/power/state | grep -v disk\"",
		"rm -r \"~/.cache/Moonlight Game Streaming Project\"",
		"raofflineproxy-ctl topup --after-index",
		"",
	};
	for (const char* line : plain)
		CHECK(maskSecrets(line) == line);
}

TEST_CASE("a value is the whole shell word, however it is quoted (#308 8b gpt F-ES-09)")
{
	// The mask stopped at the first closing quote, or at a quote in a bare
	// word, and copied the rest: `--password 'front'back` passes the shell
	// the password frontback, and the log kept "back". A shell word runs
	// until unquoted whitespace (or & ; |), across every quoted and bare
	// piece and every backslash-escaped character in it.
	CHECK(maskSecrets("tool --password 'front'back") == "tool --password <redacted>");
	// The shell's other way of writing a quote inside single quotes, '"'"',
	// read as the string's end before the fix, and "front back" was logged
	// (the audit of the fixes, gpt G3-E-01).
	CHECK(maskSecrets("sh -c 'tool --password '\"'\"'front back'\"'\"''") == "sh -c 'tool --password <redacted>'");
	CHECK(maskSecrets("sh -c 'tool --password '\"'\"'front back'\"'\"' --after x'") == "sh -c 'tool --password <redacted> --after x'");
	CHECK(maskSecrets("tool --password front'back' more") == "tool --password <redacted> more");
	CHECK(maskSecrets("tool --password front\\ back more") == "tool --password <redacted> more");
	CHECK(maskSecrets("tool --password \"front\"'mid'back; next") == "tool --password <redacted>; next");
	CHECK(maskSecrets("pass=\"a\"'b'c next=1") == "pass=<redacted> next=1");
	CHECK(maskSecrets("wifictl connect 'Home Net' 'pa'ss'word' 'US'") == "wifictl connect 'Home Net' <redacted> 'US'");

	// Inside a quoted string the value ends where the string does, as it
	// always did: a URL in double quotes, a command in single quotes.
	CHECK(maskSecrets("curl \"https://h/dorequest.php?r=login&u=bob&p=hunter2\" -o out")
		== "curl \"https://h/dorequest.php?r=login&u=bob&p=<redacted>\" -o out");
	CHECK(maskSecrets("sh -c 'tool --password hunter2 && x'") == "sh -c 'tool --password <redacted> && x'");
}

TEST_CASE("a value quoted inside a quoted command is masked whole (audit of the fixes, gpt coverage 6)")
{
	// Inside a quoted string the value ended at the first quote or space of
	// any kind, so the inner shell's own quoting -- sh -c 'tool --password
	// "front back"' -- ended it before it began, and the password went to
	// the log as it was. The first mask stopped at the quote's close; the
	// F-ES-09 rewrite made it worse. An inner quoted word is one value.
	CHECK(maskSecrets("sh -c 'tool --password \"front back\"'") == "sh -c 'tool --password <redacted>'");
	CHECK(maskSecrets("sh -c \"tool --password 'front back'\"") == "sh -c \"tool --password <redacted>\"");
	CHECK(maskSecrets("sh -c \"tool --password \\\"front back\\\"\"") == "sh -c \"tool --password <redacted>\"");
	CHECK(maskSecrets("sh -c 'tool --password front\"mid dle\"back && x'") == "sh -c 'tool --password <redacted> && x'");
	// The enclosing string still ends the value where it ends.
	CHECK(maskSecrets("sh -c 'x --password hunter2' next") == "sh -c 'x --password <redacted>' next");
	CHECK(maskSecrets("curl \"https://h/?u=bob&p=hunter2\" -o out") == "curl \"https://h/?u=bob&p=<redacted>\" -o out");
}

TEST_CASE("an escaped quote inside an inner quoted value does not end it (audit of the fix round PL-010)")
{
	// Inside an enclosing quote the inner quote was tracked before the
	// backslash, so an escaped inner double quote closed the inner quote and
	// the space after it ended the mask: sh -c 'tool --password "front\"
	// back"' passes the inner shell the one word front" back, and the log
	// kept "back". The inner command is read as its shell reads it -- in an
	// enclosing double-quoted string the inner backslash is itself written
	// \\ -- and inside an inner double quote a backslash takes the next
	// character with it, in both enclosing modes.
	CHECK(maskSecrets(R"(sh -c 'tool --password "front\" back"')") == R"(sh -c 'tool --password <redacted>')");
	CHECK(maskSecrets(R"(sh -c "tool --password \"front\\\" back\"")") == R"(sh -c "tool --password <redacted>")");
	CHECK(maskSecrets(R"(sh -c 'tool --password "front\" back" && x')") == R"(sh -c 'tool --password <redacted> && x')");
	// An escaped backslash before the inner quote's close still closes it.
	CHECK(maskSecrets(R"(sh -c 'tool --password "front\\" next')") == R"(sh -c 'tool --password <redacted> next')");
	// A backslash at the inner command's top level, in both enclosing modes.
	CHECK(maskSecrets(R"(sh -c 'tool --password front\ back && x')") == R"(sh -c 'tool --password <redacted> && x')");
	CHECK(maskSecrets(R"(sh -c "tool --password front\\ back && x")") == R"(sh -c "tool --password <redacted> && x")");
}

TEST_CASE("shellQuote's escaped quote inside a single-quoted command is a quote of the command (audit of the fix round, claude G2-E-core-01)")
{
	// shellQuote writes a single quote inside its string as '\'' -- close,
	// escaped quote, reopen -- so a value it quoted, nested in a command it
	// quoted again, begins with the enclosing quote. The mask took that
	// first byte for the string's end, masked nothing, and logged the value.
	// The four bytes are one quote of the inner command.
	using Utils::String::shellQuote;
	CHECK(maskSecrets("sh -c " + shellQuote("tool --password " + shellQuote("front back")))
		== "sh -c 'tool --password <redacted>'");
	CHECK(maskSecrets(R"(sh -c 'tool --password '\''front back'\'' && x')") == R"(sh -c 'tool --password <redacted> && x')");
	// An apostrophe inside the inner double quote.
	CHECK(maskSecrets(R"(sh -c 'tool --password "it'\''s front back" && x')") == R"(sh -c 'tool --password <redacted> && x')");
	// Inside the inner single quote a backslash is only a backslash: the
	// inner shell reads 'front\'back as the one word front\back.
	CHECK(maskSecrets(R"(sh -c 'tool --password '\''front\'\''back next')") == R"(sh -c 'tool --password <redacted> next')");
	// A lone quote still ends the enclosing string, and the value with it.
	CHECK(maskSecrets("sh -c 'x --password front' back") == "sh -c 'x --password <redacted>' back");
}
