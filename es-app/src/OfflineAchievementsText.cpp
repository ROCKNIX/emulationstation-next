#include "OfflineAchievementsText.h"
#include "utils/OfflineProxyUrl.h"

#include "utils/StringUtil.h"
#include <rapidjson/document.h>
#include <cstdlib>

namespace
{
	int jsonInt(const rapidjson::Value& val, const char* name)
	{
		if (!val.IsObject() || !val.HasMember(name))
			return 0;
		const rapidjson::Value& v = val[name];
		if (v.IsInt())
			return v.GetInt();
		if (v.IsInt64())
			return (int)v.GetInt64();
		if (v.IsString())
			return Utils::String::toInteger(v.GetString());
		return 0;
	}

	std::string jsonString(const rapidjson::Value& val, const char* name)
	{
		if (!val.IsObject() || !val.HasMember(name))
			return "";
		const rapidjson::Value& v = val[name];
		if (v.IsString())
			return v.GetString();
		if (v.IsInt())
			return std::to_string(v.GetInt());
		return "";
	}

	// A core achievement is Flags 3; 5 is unofficial and RetroArch shows it
	// to nobody, so neither does the page. A body without Flags is read as
	// core, as the proxy's own filter reads it.
	const int FlagCore = 3;

	void readAchievements(const rapidjson::Value& list, std::vector<OfflineAchievementsText::Achievement>& out)
	{
		if (!list.IsArray())
			return;
		for (const auto& item : list.GetArray())
		{
			if (!item.IsObject())
				continue;
			OfflineAchievementsText::Achievement a;
			a.id = jsonInt(item, "ID");
			if (a.id <= 0)
				continue;
			if (item.HasMember("Flags") && jsonInt(item, "Flags") != FlagCore)
				continue;
			a.title = jsonString(item, "Title");
			a.description = jsonString(item, "Description");
			a.points = jsonInt(item, "Points");
			a.badgeName = jsonString(item, "BadgeName");
			a.badgeUrl = jsonString(item, "BadgeURL");
			a.badgeLockedUrl = jsonString(item, "BadgeLockedURL");
			out.push_back(a);
		}
	}

	bool parseDocument(const std::string& body, rapidjson::Document& doc)
	{
		doc.Parse(body.c_str());
		return !doc.HasParseError() && doc.IsObject();
	}

	// One whole non-negative number and nothing else.
	bool wholeNumber(const std::string& s)
	{
		if (s.empty())
			return false;
		for (char c : s)
			if (c < '0' || c > '9')
				return false;
		return true;
	}
}

std::string OfflineAchievementsText::requestUrl(const std::string& query)
{
	return std::string(Utils::OfflineProxy::Base) + "/dorequest.php?" + query;
}

std::string OfflineAchievementsText::badgeUrl(const std::string& badgeName, bool unlocked)
{
	if (badgeName.empty())
		return "";
	return std::string(Utils::OfflineProxy::Base) + "/Badge/" + badgeName + (unlocked ? ".png" : "_lock.png");
}

OfflineAchievementsText::Game OfflineAchievementsText::parsePatch(const std::string& body)
{
	Game game;
	rapidjson::Document doc;
	if (!parseDocument(body, doc) || !doc.HasMember("PatchData") || !doc["PatchData"].IsObject())
		return game;

	const rapidjson::Value& patch = doc["PatchData"];
	game.id = jsonInt(patch, "ID");
	if (game.id <= 0)
		return game;
	game.title = jsonString(patch, "Title");
	game.imageUrl = jsonString(patch, "ImageIcon");
	// A set with nothing in it is not a cached game (PL-26).
	if (!patch.HasMember("Achievements") || !patch["Achievements"].IsArray() || patch["Achievements"].Empty())
		return game;
	readAchievements(patch["Achievements"], game.achievements);
	game.ok = true;
	return game;
}

OfflineAchievementsText::Game OfflineAchievementsText::parseAchievementSets(const std::string& body)
{
	Game game;
	rapidjson::Document doc;
	if (!parseDocument(body, doc))
		return game;

	game.id = jsonInt(doc, "GameId");
	if (game.id <= 0)
		return game;
	game.title = jsonString(doc, "Title");
	game.imageUrl = jsonString(doc, "ImageIconUrl");

	if (!doc.HasMember("Sets") || !doc["Sets"].IsArray())
		return game;
	const rapidjson::Value* chosen = nullptr;
	for (const auto& set : doc["Sets"].GetArray())
	{
		if (!set.IsObject())
			continue;
		if (chosen == nullptr)
			chosen = &set;
		if (jsonString(set, "Type") == "core")
		{
			chosen = &set;
			break;
		}
	}
	// No set, or a set with nothing in it, is not a cached game (PL-26).
	if (chosen == nullptr || !chosen->HasMember("Achievements") || !(*chosen)["Achievements"].IsArray() || (*chosen)["Achievements"].Empty())
		return game;
	readAchievements((*chosen)["Achievements"], game.achievements);
	game.ok = true;
	return game;
}

OfflineAchievementsText::StoreGame OfflineAchievementsText::parseStoreGame(const std::string& line)
{
	StoreGame game;
	rapidjson::Document doc;
	if (!parseDocument(line, doc) || !doc.IsObject())
		return game;
	game.id = jsonInt(doc, "id");
	game.achievements = jsonInt(doc, "achievements");
	if (game.id <= 0 || game.achievements <= 0)
		return game;
	game.title = jsonString(doc, "title");
	game.icon = jsonString(doc, "icon");
	game.points = jsonInt(doc, "points");
	// null is the ctl's "the unlock row could not be read" (#307 PL-057):
	// unknown, not none. A count left out altogether still reads 0.
	if (doc.HasMember("unlocked") && doc["unlocked"].IsNull())
		game.unlockedKnown = false;
	else
	{
		game.unlocked = jsonInt(doc, "unlocked");
		game.unlockedPoints = jsonInt(doc, "unlockedPoints");
	}
	game.pending = jsonInt(doc, "pending");
	game.ok = true;
	return game;
}

OfflineAchievementsText::Unlocks OfflineAchievementsText::parseUnlocks(const std::string& body)
{
	Unlocks unlocks;
	rapidjson::Document doc;
	if (!parseDocument(body, doc) || !doc.HasMember("UserUnlocks") || !doc["UserUnlocks"].IsArray())
		return unlocks;
	// Success false with a list would be the proxy contradicting itself;
	// not the shape either.
	if (doc.HasMember("Success") && doc["Success"].IsBool() && !doc["Success"].GetBool())
		return unlocks;
	for (const auto& v : doc["UserUnlocks"].GetArray())
	{
		int id = 0;
		if (v.IsInt())
			id = v.GetInt();
		else if (v.IsString())
			id = Utils::String::toInteger(v.GetString());
		if (id > 0)
			unlocks.ids.push_back(id);
	}
	unlocks.ok = true;
	return unlocks;
}

std::string OfflineAchievementsText::parseError(const std::string& body)
{
	rapidjson::Document doc;
	if (!parseDocument(body, doc))
		return "";
	if (doc.HasMember("Success") && doc["Success"].IsBool() && doc["Success"].GetBool())
		return "";
	return jsonString(doc, "Error");
}

bool OfflineAchievementsText::isNotCached(const std::string& body)
{
	return parseError(body) == "no cached response";
}

std::vector<OfflineAchievementsText::PendingAward> OfflineAchievementsText::parsePendingIds(const std::string& text)
{
	std::vector<PendingAward> out;
	for (const std::string& raw : Utils::String::split(text, '\n', true))
	{
		const std::string line = Utils::String::trim(raw);
		if (line.empty())
			continue;
		auto fields = Utils::String::split(line, ' ', true);
		if (fields.size() != 2 || !wholeNumber(fields[0]) || !wholeNumber(fields[1]))
			continue;
		PendingAward award;
		award.id = Utils::String::toInteger(fields[0]);
		award.when = (time_t)strtoll(fields[1].c_str(), nullptr, 10);
		if (award.id <= 0)
			continue;
		out.push_back(award);
	}
	return out;
}

bool OfflineAchievementsText::parseOnlineState(const std::string& text, bool& online)
{
	rapidjson::Document doc;
	if (!parseDocument(text, doc) || !doc.HasMember("online") || !doc["online"].IsBool())
		return false;
	online = doc["online"].GetBool();
	return true;
}

std::vector<int> OfflineAchievementsText::parseReadyIds(const std::string& text)
{
	std::vector<int> ids;
	for (const std::string& raw : Utils::String::split(text, '\n', true))
	{
		const std::string line = Utils::String::trim(raw);
		if (!wholeNumber(line))
			continue;
		int id = Utils::String::toInteger(line);
		if (id > 0)
			ids.push_back(id);
	}
	return ids;
}

OfflineAchievementsText::AccountTotals OfflineAchievementsText::parseAccountTotals(const std::string& text)
{
	AccountTotals totals;
	bool score = false, softcore = false;
	for (const std::string& raw : Utils::String::split(Utils::String::trim(text), ' ', true))
	{
		const std::string field = Utils::String::trim(raw);
		if (Utils::String::startsWith(field, "score=") && wholeNumber(field.substr(6)))
		{
			totals.score = Utils::String::toInteger(field.substr(6));
			score = true;
		}
		else if (Utils::String::startsWith(field, "softcore=") && wholeNumber(field.substr(9)))
		{
			totals.softcore = Utils::String::toInteger(field.substr(9));
			softcore = true;
		}
	}
	totals.ok = score && softcore;
	return totals;
}
