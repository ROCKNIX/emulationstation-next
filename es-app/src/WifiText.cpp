#include "WifiText.h"

// Only a CR: the name is the rest of the line, spaces and all.
static std::string withoutCR(const std::string& line)
{
	if (!line.empty() && line.back() == '\r')
		return line.substr(0, line.size() - 1);
	return line;
}

bool WifiText::parseSavedLine(const std::string& rawLine, SavedNetwork& network)
{
	const std::string line = withoutCR(rawLine);
	const size_t tab = line.rfind('\t');
	if (tab == std::string::npos)
		return false;

	const std::string name = line.substr(0, tab);
	const std::string flag = line.substr(tab + 1);
	if (name.empty())
		return false;

	if (flag == "active")
		network.inUse = true;
	else if (flag == "saved")
		network.inUse = false;
	else
		return false;

	network.name = name;
	return true;
}

std::vector<WifiText::SavedNetwork> WifiText::parseSaved(const std::vector<std::string>& lines)
{
	std::vector<SavedNetwork> networks;
	for (const auto& line : lines)
	{
		SavedNetwork network;
		if (parseSavedLine(line, network))
			networks.push_back(network);
	}
	return networks;
}

std::string WifiText::parseCurrent(const std::vector<std::string>& lines)
{
	for (const auto& line : lines)
	{
		const std::string ssid = withoutCR(line);
		if (!ssid.empty())
			return ssid;
	}
	return "";
}

std::vector<WifiText::PickerRow> WifiText::pickerRows(const std::vector<std::string>& inRange, const std::vector<SavedNetwork>& saved, const std::string& current,
	bool savedKnown, bool currentKnown)
{
	// The device's network when `wifictl current` could not say: the saved
	// list marks the profile that is active (#308 F-WF-03).
	std::string joinedNow = current;
	if (!currentKnown && savedKnown)
		for (const auto& network : saved)
			if (network.inUse)
			{
				joinedNow = network.name;
				break;
			}

	auto isSaved = [&saved](const std::string& name)
	{
		for (const auto& network : saved)
			if (network.name == name)
				return true;
		return false;
	};

	std::vector<PickerRow> rows;
	auto listed = [&rows](const std::string& name)
	{
		for (const auto& row : rows)
			if (row.name == name)
				return true;
		return false;
	};

	// The profile the device is on (#308 claude F-WF-12, gpt F-WF-03).
	// wifictl speaks two names: current the SSID, saved and join a profile's
	// name, and the two differ for a renamed profile or NetworkManager's
	// "Home 1". So the connected row joins by a profile, in this order:
	//   - the one named as its SSID, when that one is up;
	//   - else the one profile that is up -- even beside a namesake that is
	//     not: the name used to decide first, and an old "Home" left saved
	//     beside the "Home 1" in use took the row's FORGET and JOIN (audit of
	//     the fix round, gpt G2-E-app-01);
	//   - else, with none up (the list's flags behind the join), the one
	//     named as the SSID, as before.
	// Two or more up and none of them named so (a second adapter: saved's
	// ACTIVE is any adapter's, where current is this device's) is not
	// guessed at, a namesake that is not up included -- it is the one
	// profile known not to be this connection -- and the row joins by its
	// name, as before.
	std::string joinedProfile;
	if (!joinedNow.empty() && savedKnown)
	{
		int up = 0;
		bool namedUp = false;
		std::string theOneUp;
		for (const auto& network : saved)
			if (network.inUse)
			{
				up++;
				theOneUp = network.name;
				if (network.name == joinedNow)
					namedUp = true;
			}
		if (namedUp)
			joinedProfile = joinedNow;
		else if (up == 1)
			joinedProfile = theOneUp;
		else if (up == 0 && isSaved(joinedNow))
			joinedProfile = joinedNow;
	}

	if (!joinedNow.empty())
	{
		PickerRow row{ joinedNow, !joinedProfile.empty(), true, savedKnown };
		row.profile = joinedProfile;
		rows.push_back(row);
	}

	for (const auto& rawName : inRange)
	{
		const std::string name = withoutCR(rawName);
		if (name.empty() || listed(name))
			continue;
		PickerRow row{ name, savedKnown && isSaved(name), false, savedKnown };
		if (row.saved)
			row.profile = name;
		rows.push_back(row);
	}
	return rows;
}

bool WifiText::parseJoin(const std::vector<std::string>& lines)
{
	for (const auto& rawLine : lines)
		if (withoutCR(rawLine) == "joined")
			return true;
	return false;
}

WifiText::JoinFailure WifiText::joinFailure(int exitCode)
{
	return exitCode == 2 ? JoinFailure::ServiceNotAnswering : JoinFailure::MayBeKey;
}

WifiText::ForgetOutcome WifiText::parseForget(const std::vector<std::string>& lines)
{
	ForgetOutcome outcome;
	for (const auto& rawLine : lines)
	{
		const std::string line = withoutCR(rawLine);
		if (line == "forgotten")
			outcome.forgotten = true;
		else if (line == "disconnected" && outcome.forgotten)
			outcome.disconnected = true;
	}
	return outcome;
}

WifiText::PressAction WifiText::pressAction(const PickerRow& row)
{
	if (row.connected)
		return PressAction::Join;
	if (row.saved)
		return PressAction::ChooseSaved;
	if (!row.savedKnown)
		return PressAction::CheckAgain;
	return PressAction::AskKey;
}

WifiText::PressAction WifiText::manualAction(const std::string& name, const std::string& current, const std::vector<SavedNetwork>& saved, bool savedKnown)
{
	if (!name.empty() && name == current)
		return PressAction::Join;
	for (const auto& network : saved)
		if (network.name == name)
			return PressAction::Join;
	if (!savedKnown)
		return PressAction::CheckAgain;
	return PressAction::AskKey;
}

// A row joins by its profile when it has one, and by its name when not --
// the connected row whose profile could not be told, which join then
// answers for itself.
std::string WifiText::joinName(const PickerRow& row)
{
	return row.profile.empty() ? row.name : row.profile;
}

// A typed name is taken as a row would be: the network the device is on
// joins by its profile, anything else by the name as typed.
std::string WifiText::manualJoinName(const std::string& name, const std::string& current, const std::string& currentProfile)
{
	if (!name.empty() && name == current && !currentProfile.empty())
		return currentProfile;
	return name;
}

std::string WifiText::joinedNotice(const std::string& name, const std::string& connectedWord)
{
	return name + " : " + connectedWord;
}
