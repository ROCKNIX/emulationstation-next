// The Wi-Fi rows' pure text (fork #191), checked without a device.
//
// Runs against es-app/src/WifiText.cpp alone: the lines wifictl prints for
// the interface -- saved, current, forget -- including the ones nobody
// meant to write. A parser is judged on its junk, so most of these are junk.

#include "doctest/doctest.h"

#include "WifiText.h"

#include <string>
#include <type_traits>
#include <vector>

using namespace WifiText;

TEST_CASE("parseSaved: two remembered networks, the one in use first, as wifictl saved prints them")
{
	auto networks = parseSaved({ "Home Wi-Fi\tactive", "Cafe: Guest\tsaved" });
	REQUIRE(networks.size() == 2);
	CHECK(networks[0].name == "Home Wi-Fi");
	CHECK(networks[0].inUse);
	CHECK(networks[1].name == "Cafe: Guest");
	CHECK_FALSE(networks[1].inUse);
}

TEST_CASE("parseSavedLine keeps a name as NetworkManager has it: case, inner spaces, a colon, an edge space")
{
	SavedNetwork network;
	CHECK(parseSavedLine("MyHome_5g\tsaved", network));
	CHECK(network.name == "MyHome_5g");            // not upper-cased: the name is case-sensitive
	CHECK(parseSavedLine("  two  spaces \tactive", network));
	CHECK(network.name == "  two  spaces ");        // every space kept, the flag read after the tab
	CHECK(network.inUse);
	CHECK(parseSavedLine("a\tb\tsaved", network));  // a tab inside a name: the flag is after the last one
	CHECK(network.name == "a\tb");
	CHECK_FALSE(network.inUse);
}

TEST_CASE("parseSavedLine tolerates a trailing CR and nothing else that is not a network")
{
	SavedNetwork network;
	CHECK(parseSavedLine("Home\tactive\r", network));
	CHECK(network.name == "Home");
	CHECK(network.inUse);

	CHECK_FALSE(parseSavedLine("", network));
	CHECK_FALSE(parseSavedLine("Home", network));               // no tab
	CHECK_FALSE(parseSavedLine("\tactive", network));           // no name
	CHECK_FALSE(parseSavedLine("Home\tyes", network));          // nmcli's own word, not wifictl's
	CHECK_FALSE(parseSavedLine("Home\tACTIVE", network));       // the flag is exact
	CHECK_FALSE(parseSavedLine("Home\tactive extra", network));
	CHECK_FALSE(parseSavedLine("Home\t", network));
}

TEST_CASE("parseSaved passes junk lines over and keeps the order of the rest")
{
	auto networks = parseSaved({ "", "Error: NetworkManager is not running.", "B\tsaved", "junk\tmaybe", "A\tactive", "\tsaved" });
	REQUIRE(networks.size() == 2);
	CHECK(networks[0].name == "B");
	CHECK(networks[1].name == "A");
	CHECK(networks[1].inUse);
	CHECK(parseSaved({}).empty());
	CHECK(parseSaved({ "", "\r" }).empty());
}

TEST_CASE("parseCurrent: the first line is the SSID, CR dropped, spaces kept; none is empty")
{
	CHECK(parseCurrent({ "Home Wi-Fi" }) == "Home Wi-Fi");
	CHECK(parseCurrent({ "Home Wi-Fi\r" }) == "Home Wi-Fi");
	CHECK(parseCurrent({ " edge " }) == " edge ");
	CHECK(parseCurrent({ "", "Late" }) == "Late");      // an empty first line is not an answer
	CHECK(parseCurrent({}) == "");
	CHECK(parseCurrent({ "", "\r" }) == "");
}

TEST_CASE("parseForget reads the word, not the absence of an error")
{
	auto both = parseForget({ "forgotten", "disconnected" });
	CHECK(both.forgotten);
	CHECK(both.disconnected);

	auto one = parseForget({ "forgotten" });
	CHECK(one.forgotten);
	CHECK_FALSE(one.disconnected);

	auto none = parseForget({});
	CHECK_FALSE(none.forgotten);
	CHECK_FALSE(none.disconnected);

	// A "disconnected" with nothing forgotten before it is not a forget.
	auto orphan = parseForget({ "disconnected" });
	CHECK_FALSE(orphan.forgotten);
	CHECK_FALSE(orphan.disconnected);

	auto junk = parseForget({ "Error: unknown connection 'x'.", "FORGOTTEN", "forgotten " });
	CHECK_FALSE(junk.forgotten);

	auto cr = parseForget({ "forgotten\r", "disconnected\r" });
	CHECK(cr.forgotten);
	CHECK(cr.disconnected);
}

TEST_CASE("pickerRows: the joined network first, the rest in the scan's order, the saved ones marked")
{
	auto rows = pickerRows({ "Cafe: Guest", "Home Wi-Fi", "Library" }, { { "Home Wi-Fi", true }, { "Cafe: Guest", false } }, "Home Wi-Fi");
	REQUIRE(rows.size() == 3);
	CHECK(rows[0].name == "Home Wi-Fi");
	CHECK(rows[0].connected);
	CHECK(rows[0].saved);
	CHECK(rows[1].name == "Cafe: Guest");
	CHECK_FALSE(rows[1].connected);
	CHECK(rows[1].saved);
	CHECK(rows[2].name == "Library");
	CHECK_FALSE(rows[2].connected);
	CHECK_FALSE(rows[2].saved);
}

TEST_CASE("pickerRows: a saved network out of range is not a row; the joined one is, even when the scan missed it")
{
	auto rows = pickerRows({ "Library" }, { { "Home Wi-Fi", true }, { "Office", false } }, "Home Wi-Fi");
	REQUIRE(rows.size() == 2);
	CHECK(rows[0].name == "Home Wi-Fi");
	CHECK(rows[0].connected);
	CHECK(rows[1].name == "Library");
	CHECK_FALSE(rows[1].saved);
}

TEST_CASE("pickerRows drops empty names and repeats, keeps a name's case, and has no connected row when the device is on none")
{
	auto rows = pickerRows({ "", "Cafe: Guest\r", "Cafe: Guest", "cafe: guest" }, {}, "");
	REQUIRE(rows.size() == 2);
	CHECK(rows[0].name == "Cafe: Guest");
	CHECK_FALSE(rows[0].connected);
	CHECK_FALSE(rows[0].saved);
	CHECK(rows[1].name == "cafe: guest");
}

TEST_CASE("parseJoin reads the word, not the absence of an error")
{
	CHECK(parseJoin({ "joined" }));
	CHECK(parseJoin({ "joined\r" }));
	CHECK_FALSE(parseJoin({}));
	CHECK_FALSE(parseJoin({ "" }));
	CHECK_FALSE(parseJoin({ "Error: Connection activation failed." }));
}

// ------------------------------------------------------ #308 F-WF-03/05/06/08

TEST_CASE("pickerRows: a saved list that could not be asked leaves every row unknown, not unsaved (#308 2 claude F-WF-03, gpt F-WF-06)")
{
	// wifictl saved exits 1 when NetworkManager cannot be asked, "so a
	// caller cannot read a silence as none"; the picker read it as none, and
	// a press on the player's own network asked for a key and rebuilt its
	// profile from what was typed.
	auto rows = pickerRows({ "Home Wi-Fi", "Library" }, {}, "Home Wi-Fi", false, true);
	REQUIRE(rows.size() == 2);
	CHECK(rows[0].connected);
	CHECK_FALSE(rows[0].savedKnown);
	CHECK_FALSE(rows[1].savedKnown);
	CHECK_FALSE(rows[1].saved);

	// And an answered list is known, as it always was.
	auto known = pickerRows({ "Library" }, {}, "", true, true);
	REQUIRE(known.size() == 1);
	CHECK(known[0].savedKnown);
}

TEST_CASE("pickerRows: a current that could not be asked takes the connected row from the saved list's active profile (#308 F-WF-03)")
{
	// wifictl current exits 2 when NetworkManager cannot be asked; the saved
	// list already says which profile is active.
	auto rows = pickerRows({ "Cafe: Guest", "Home Wi-Fi" }, { { "Home Wi-Fi", true }, { "Cafe: Guest", false } }, "", true, false);
	REQUIRE(rows.size() == 2);
	CHECK(rows[0].name == "Home Wi-Fi");
	CHECK(rows[0].connected);
	CHECK(rows[0].saved);
	CHECK(rows[1].name == "Cafe: Guest");
	CHECK_FALSE(rows[1].connected);
}

TEST_CASE("a press: the connected row is checked by a join, a saved one joins, an unknown one is asked again (#308 F-WF-03/05/06)")
{
	// The connected row closed the picker on the snapshot it was built from,
	// with no look at whether the device was still on it (gpt F-WF-05).
	// wifictl join answers "joined" at once for the active profile and brings
	// back one that has dropped, so a press on it goes that way.
	PickerRow connected{ "Home Wi-Fi", true, true };
	CHECK(pressAction(connected) == PressAction::Join);
	// A saved network the device is not on is offered CONNECT / FORGET /
	// CANCEL (#318, D-UI-118); the connected one, saved or not, joins at once.
	PickerRow saved{ "Cafe: Guest", true, false };
	CHECK(pressAction(saved) == PressAction::ChooseSaved);
	PickerRow savedConnected{ "Home Wi-Fi", true, true };
	CHECK(pressAction(savedConnected) == PressAction::Join);
	PickerRow other{ "Library", false, false };
	CHECK(pressAction(other) == PressAction::AskKey);
	PickerRow unknown{ "Library", false, false, false };
	CHECK(pressAction(unknown) == PressAction::CheckAgain);
	PickerRow unknownConnected{ "Home Wi-Fi", false, true, false };
	CHECK(pressAction(unknownConnected) == PressAction::Join);   // the device is on it: it has a profile

	// INPUT MANUALLY, the same rules for a typed name.
	const std::vector<SavedNetwork> profiles = { { "Home Wi-Fi", true }, { "Hidden", false } };
	CHECK(manualAction("Home Wi-Fi", "Home Wi-Fi", profiles, true) == PressAction::Join);
	CHECK(manualAction("Hidden", "Home Wi-Fi", profiles, true) == PressAction::Join);
	CHECK(manualAction("Guest", "Home Wi-Fi", profiles, true) == PressAction::AskKey);
	CHECK(manualAction("Guest", "Home Wi-Fi", {}, false) == PressAction::CheckAgain);
	CHECK(manualAction("", "", {}, true) == PressAction::AskKey);
}

TEST_CASE("the joined toast is <subject> : <outcome>, the name as it is (#308 2 claude F-WF-08, gpt F-WF-08)")
{
	// It was "CONNECTED TO" + name: a translated fragment a translation could
	// not move, and not the toast shape the style guide sets.
	CHECK(joinedNotice("Home Wi-Fi", "CONNECTED") == "Home Wi-Fi : CONNECTED");
	CHECK(joinedNotice("cafe guest", "CONNECT\xC3\x89") == "cafe guest : CONNECT\xC3\x89");
}


TEST_CASE("a join that did not happen: NetworkManager not answering is not a key that changed (#308 2 claude F-WF-03, gpt F-WF-06)")
{
	// wifictl join: exit 2 when NetworkManager could not be asked -- nothing
	// was tried, and the key is not in question. The picker told the player
	// to forget the network and join it again with a new key.
	CHECK(joinFailure(2) == JoinFailure::ServiceNotAnswering);
	// Exit 1: the profile would not come up, or the name is not a saved
	// network -- the key may be why. 124 is the timeout's own bound on the
	// activation: the same may.
	CHECK(joinFailure(1) == JoinFailure::MayBeKey);
	CHECK(joinFailure(124) == JoinFailure::MayBeKey);
	// A run that exited 0 without printing "joined" (parseJoin) is not a
	// join; nothing in it points away from the key either.
	CHECK(joinFailure(0) == JoinFailure::MayBeKey);
}

// wifictl speaks two names (#308 2-wifi claude F-WF-12, gpt F-WF-03): current
// and list the SSID, saved and join the profile's name -- which need not be
// the SSID (a renamed profile, or NetworkManager's "Home 1"). Stream B's
// join now writes the SSID to wifi.ssid and is judged on this device's
// adapter; the picker still handed join the SSID for the network the device
// is on, which is no profile's name, and said COULDN'T CONNECT TO HOME with
// forget-and-rejoin advice about the network the player was on.
TEST_CASE("the connected row joins by the profile that is up, not by its SSID (#308 2 claude F-WF-12, gpt F-WF-03)")
{
	const std::vector<SavedNetwork> profiles = { { "Home 1", true }, { "Cafe", false } };
	auto rows = pickerRows({ "Home", "Cafe", "Library" }, profiles, "Home", true, true);
	REQUIRE(rows.size() == 3);
	CHECK(rows[0].name == "Home");        // the SSID, as the player knows it
	CHECK(rows[0].connected);
	CHECK(rows[0].saved);                 // joined through a profile: it has one
	CHECK(rows[0].profile == "Home 1");
	CHECK(joinName(rows[0]) == "Home 1");
	CHECK(rows[1].name == "Cafe");
	CHECK(joinName(rows[1]) == "Cafe");   // a profile named as its SSID
	CHECK(joinName(rows[2]) == "Library");

	// Typed: the current network's name joins by its profile too.
	CHECK(manualJoinName("Home", "Home", "Home 1") == "Home 1");
	CHECK(manualJoinName("Cafe", "Home", "Home 1") == "Cafe");
	CHECK(manualJoinName("Hidden", "Home", "") == "Hidden");

	// A profile named as the SSID is the one, even with another up (a second
	// adapter): the name decides before the active flag.
	auto two = pickerRows({ "Home" }, { { "Work", true }, { "Home", true } }, "Home", true, true);
	REQUIRE(two.size() == 1);
	CHECK(two[0].profile == "Home");
	// Two up and neither named as the SSID: no guess; the row joins by its name.
	auto ambiguous = pickerRows({ "Home" }, { { "Work", true }, { "Home 1", true } }, "Home", true, true);
	REQUIRE(ambiguous.size() == 1);
	CHECK(ambiguous[0].profile.empty());
	CHECK(joinName(ambiguous[0]) == "Home");

	// Could not ask which are saved: no profile is claimed.
	auto unknown = pickerRows({ "Home" }, {}, "Home", false, true);
	REQUIRE(unknown.size() == 1);
	CHECK(unknown[0].profile.empty());
	CHECK_FALSE(unknown[0].saved);
}

// The audit of the fix round, gpt G2-E-app-01 (handed to stream E1): the
// name decided before the active flag, so a saved profile named as the SSID
// but not in use outranked the one profile that was -- NetworkManager's
// "Home 1" beside an old "Home" -- and the connected row's FORGET and JOIN
// went to the profile the device was not on.
TEST_CASE("the connected row's profile is the one in use, over an inactive namesake (audit of the fix round, gpt G2-E-app-01)")
{
	auto rows = pickerRows({ "Home" }, { { "Home", false }, { "Home 1", true } }, "Home", true, true);
	REQUIRE(rows.size() == 1);
	CHECK(rows[0].connected);
	CHECK(rows[0].profile == "Home 1");
	CHECK(joinName(rows[0]) == "Home 1");

	// Two up (a second adapter) and neither named as the SSID: which one is
	// this device's cannot be told, and the namesake is known not to be in
	// use, so none is claimed.
	auto twoUp = pickerRows({ "Home" }, { { "Home", false }, { "Work", true }, { "Home 1", true } }, "Home", true, true);
	REQUIRE(twoUp.size() == 1);
	CHECK(twoUp[0].profile.empty());
	CHECK(joinName(twoUp[0]) == "Home");

	// None up -- the list's flags behind the join -- and a namesake: the
	// namesake, as before.
	auto noneUp = pickerRows({ "Home" }, { { "Home", false } }, "Home", true, true);
	REQUIRE(noneUp.size() == 1);
	CHECK(noneUp[0].profile == "Home");
}

// Audit of the fixes (#307), E2 claude G-E2-01: joinWifiNetwork returned
// bool (true = joined) and returns the exit code now (0 = joined), so a
// caller left testing it as a truth value compiles and reads every join
// backwards. The answer is a type that does not turn into a bool.
TEST_CASE("a join's answer is not a truth value (#308 F-WF-03/06; audit G-E2-01)")
{
	static_assert(!std::is_convertible<JoinAnswer, bool>::value, "a join's answer must not read as a truth value");
	static_assert(!std::is_constructible<bool, JoinAnswer>::value, "nor be cast to one");
	CHECK(JoinAnswer{ 0 }.joined());
	CHECK_FALSE(JoinAnswer{ 1 }.joined());
	CHECK_FALSE(JoinAnswer{ 2 }.joined());
	CHECK_FALSE(JoinAnswer().joined());
	CHECK(joinFailure(JoinAnswer{ 2 }.code) == JoinFailure::ServiceNotAnswering);
}
