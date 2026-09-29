#pragma once
#ifndef ES_APP_WIFI_TEXT_H
#define ES_APP_WIFI_TEXT_H

// The pure text of the Wi-Fi rows (fork #191): what wifictl prints for the
// interface, read into values the pages can show, with nothing from the
// window, the settings or a script behind it, so es-unit-tests can hold the
// rules (es-app/tests/unit). The thin shells that run wifictl are in
// ApiSystem; the rows themselves are in GuiMenu.
//
// The setting wifi.ssid is the network the player configured last.
// NetworkManager keeps a profile for every network the device has joined and
// autoconnects to whichever remembered one is in range, so what the device is
// on and what the setting says are two different facts; these are the shapes
// wifictl reports the first in.

#include <string>
#include <vector>

namespace WifiText
{
	// A network NetworkManager remembers, as one line of `wifictl saved`
	// reads: <name><TAB>active or <name><TAB>saved.
	struct SavedNetwork
	{
		std::string name;
		bool inUse = false;
	};

	// One line of `wifictl saved`. False for a line that is not a network:
	// no tab, an empty name, a flag that is neither word. A trailing CR is
	// tolerated. The name keeps its case and every space in it -- a network's
	// name is case-sensitive, and is what the player recognises it by -- and
	// the flag is read after the last tab, so a tab inside a name still
	// leaves the flag whole.
	bool parseSavedLine(const std::string& line, SavedNetwork& network);

	// Every network in the lines, in the order given (nmcli puts the one in
	// use first); lines that are not networks are passed over.
	std::vector<SavedNetwork> parseSaved(const std::vector<std::string>& lines);

	// The first line of `wifictl current`, the SSID the device is joined to;
	// empty when there is none. Only a trailing CR is trimmed: a name may
	// begin or end with a space.
	std::string parseCurrent(const std::vector<std::string>& lines);

	// A row of the Wi-Fi picker (fork #191; the maintainer's paradigm of
	// 2026-09-15: the WI-FI NETWORK row -- WI-FI SSID until D-UI-071 renamed
	// it for its new value -- is the network the device is on, the
	// list behind it is what is in range, and a saved one joins with a
	// press). Built by pickerRows from what `wifictl list` found, the
	// profiles `wifictl saved` holds and the network `wifictl current`
	// answered.
	struct PickerRow
	{
		std::string name;
		bool saved = false;
		bool connected = false;
		// Whether `saved` is an answer: false when `wifictl saved` could
		// not be asked (#308 F-WF-03/06), and the row is then neither saved
		// nor unsaved, only unknown.
		bool savedKnown = true;
		// The saved profile a press joins by, as `wifictl join` takes it;
		// empty when the row has none known.
		std::string profile;
	};

	// The rows in the order the list shows them: the network joined now
	// first (added when the scan missed it -- a hidden network is joined and
	// not listed), then the rest in the scan's order, empty names and repeats
	// dropped, each marked saved when NetworkManager holds a profile for it.
	// Saved networks out of range are not rows: this list is what can be
	// joined from here; MANAGE SAVED NETWORKS lists them all. Names compare
	// exactly, as NetworkManager does.
	// savedKnown and currentKnown say whether `wifictl saved` and `wifictl
	// current` answered (ApiSystem's two getters return it): an empty list
	// and no list are different answers (#308 F-WF-03/06).
	std::vector<PickerRow> pickerRows(const std::vector<std::string>& inRange, const std::vector<SavedNetwork>& saved, const std::string& current,
		bool savedKnown = true, bool currentKnown = true);

	// What a press on a row does, and what INPUT MANUALLY does with a name.
	//
	// The network the device is on is joined too, not taken on trust
	// (#308 F-WF-05): the row was built from a snapshot, and the device may
	// have dropped it or moved to another since. wifictl join answers
	// "joined" at once for a profile that is active and brings back one that
	// is not, so the press confirms or repairs, and the picker closes only
	// on its answer. A network whose profile could not be asked about is
	// asked about again, never taken for one with no profile: the key path
	// (wifictl connect) deletes and rebuilds a profile of that name (#308
	// F-WF-03/06).
	//
	// A saved network the device is not on is a choice, not a join (#318,
	// D-UI-118, taken from ROCKNIX's own saved-Wi-Fi work): CONNECT with the
	// key NetworkManager holds, or FORGET it. The connected row still joins
	// at once -- the press confirms or repairs -- and a typed name that is
	// saved still joins, since typing it is asking to join it.
	enum class PressAction
	{
		Join,         // join by the profile NetworkManager holds (wifictl join)
		ChooseSaved,  // a saved network not in use: CONNECT / FORGET / CANCEL
		AskKey,       // a network with no profile: its key, then connect
		CheckAgain    // whether it has a profile could not be asked: never assume it has none
	};
	PressAction pressAction(const PickerRow& row);
	PressAction manualAction(const std::string& name, const std::string& current, const std::vector<SavedNetwork>& saved, bool savedKnown);

	// The name `wifictl join` is handed for a row's press, and for a typed
	// name (current is the connected row's name, currentProfile its
	// profile).
	std::string joinName(const PickerRow& row);
	std::string manualJoinName(const std::string& name, const std::string& current, const std::string& currentProfile);

	// The toast when the picker has joined a network, in the toast's shape
	// (es-ui-style-guide.md: <glyph> <subject> : <outcome>): the name as
	// NetworkManager has it, then the word the row uses for the same fact
	// (#308 F-WF-08). The caller puts the glyph in front.
	std::string joinedNotice(const std::string& name, const std::string& connectedWord);

	// What `wifictl join` printed: "joined" once the saved network's profile
	// is active. Anything else, or nothing, is not a join that happened.
	bool parseJoin(const std::vector<std::string>& lines);

	// What a join that did not happen means, from `wifictl join`'s exit code
	// (#308 2-wifi claude F-WF-03, gpt F-WF-06): 2 is NetworkManager not
	// answering -- nothing was tried, and the key is not in question; any
	// other (1: the profile would not come up or is not saved, 124: the
	// timeout's bound on the activation, 0 without "joined") may be the key.
	enum class JoinFailure { ServiceNotAnswering, MayBeKey };
	JoinFailure joinFailure(int exitCode);

	// wifictl join's answer, as ApiSystem::joinWifiNetwork gives it: the
	// exit code (0 joined; 1 not joined; 2 NetworkManager not answering;
	// 124 the timeout's bound), in a type that does not turn into a bool.
	// The call returned bool, true for joined, until #308 F-WF-03/06, and 0
	// means joined now: a caller left testing it as a truth value would
	// compile and read every join backwards (audit of the fixes, E2 claude
	// G-E2-01). This one does not compile.
	struct JoinAnswer
	{
		int code = 1;
		bool joined() const { return code == 0; }
	};

	// What `wifictl forget` printed: "forgotten" when the profile went, then
	// "disconnected" on a second line when it was the one in use. A
	// "disconnected" with no "forgotten" before it is not a forget that
	// happened; nothing printed is not one either -- a caller reads the
	// word, not the absence of an error.
	struct ForgetOutcome
	{
		bool forgotten = false;
		bool disconnected = false;
	};
	ForgetOutcome parseForget(const std::vector<std::string>& lines);
}

#endif // ES_APP_WIFI_TEXT_H
