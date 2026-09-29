#pragma once


#include <string>
#include "HttpReq.h"
#include <vector>
#include <map>

#include "ApiSystem.h"
#include "OfflineAchievementsText.h"

class SystemData;

// API_GetGameInfoAndUserProgress

struct Achievement
{
	std::string ID;
	std::string NumAwarded;
	std::string NumAwardedHardcore;
	std::string Title;
	std::string Description;
	std::string Points;
	std::string TrueRatio;
	std::string Author;
	std::string DateModified;
	std::string DateCreated;
	std::string BadgeName;
	int DisplayOrder = 0;
	std::string MemAddr;
	std::string DateEarned;
	std::string DateEarnedHardcore;

	// From the device (fork #180): the offline proxy's cache says which
	// achievements are unlocked but not when, so an unlock read from it has
	// no date; and one still queued for RetroAchievements is Pending as well.
	bool UnlockedOnDevice = false;
	bool Pending = false;
	// The badge as the proxy rewrote it (http://127.0.0.1:8080/Badge/...);
	// empty for the web API, whose badges are built from BadgeName.
	std::string BadgeUrl;
	std::string BadgeLockedUrl;

	bool isUnlocked() const;
	std::string getBadgeUrl();
};

struct GameInfoAndUserProgress
{
	int ID;
	std::string Title;
	int ConsoleID;
	int ForumTopicID;
	int Flags;
	std::string ImageIcon;
	std::string ImageTitle;
	std::string ImageIngame;
	std::string ImageBoxArt;
	std::string Publisher;
	std::string Developer;
	std::string Genre;
	std::string Released;
	bool IsFinal;
	std::string ConsoleName;
	std::string RichPresencePatch;
	int NumAchievements;
	std::string NumDistinctPlayersCasual;
	std::string NumDistinctPlayersHardcore;
	std::vector<Achievement> Achievements;
	int NumAwardedToUser;
	int NumAwardedToUserHardcore;
	std::string UserCompletion;
	std::string UserCompletionHardcore;

	// Read from the offline proxy's cache on this device rather than from
	// RetroAchievements (fork #180, D-RA-009); the page says so.
	bool FromDevice = false;
	// The device is offline and the proxy has never cached this game: the
	// page shows the one line that says what to do, not an empty list.
	bool NotOnDevice = false;
	// The proxy did not answer at all -- nothing listening, a timeout, a
	// body that was not its own -- as against a miss it answered with. A
	// loop over many games stops at the first of these rather than paying
	// the timeout once per game (audit #186 PL-09).
	bool ProxyDidNotAnswer = false;

	std::string getImageUrl(const std::string& image = "");
};


// API_GetUserSummary
/*

struct LastActivity
{
	std::string ID;
	std::string timestamp;
	std::string lastupdate;
	std::string activitytype;
	std::string User;
	std::string data;
	std::string data2;
};

struct LastGame
{
	int ID;
	std::string Title;
	int ConsoleID;
	int ForumTopicID;
	int Flags;
	std::string ImageIcon;
	std::string ImageTitle;
	std::string ImageIngame;
	std::string ImageBoxArt;
	std::string Publisher;
	std::string Developer;
	std::string Genre;
	std::string Released;
	bool IsFinal;
	std::string ConsoleName;
	std::string RichPresencePatch;
};
*/
struct Award
{
	int NumPossibleAchievements;
	int PossibleScore;
	int NumAchieved;
	int NumAchievedHardcore;

	int ScoreAchieved;	
	int ScoreAchievedHardcore;

	// The device's summary could not read what was earned (the store's
	// unlock row missing or unreadable, #307 PL-057): NumAchieved and
	// ScoreAchieved are 0 and mean nothing.
	bool AchievedUnknown = false;
};

struct RecentGame
{
	std::string GameID;
	std::string ConsoleID;
	std::string ConsoleName;
	std::string Title;
	std::string ImageIcon;
	std::string LastPlayed;
	std::string MyVote;
};

struct RecentAchievement
{
	std::string ID;
	std::string GameID;
	std::string GameTitle;
	std::string Title;
	std::string Description;
	std::string Points;
	std::string BadgeName;
	std::string IsAwarded;
	std::string DateAwarded;
	std::string HardcoreAchieved;
};

struct UserSummary
{
	std::string Username;

	std::string getBadge() {
		return "https://retroachievements.org" + UserPic;
	}

	int RecentlyPlayedCount;
	std::vector<RecentGame> RecentlyPlayed;
	std::string MemberSince;
//	LastActivity LastActivity;
	std::string RichPresenceMsg;
	std::string LastGameID;
//	LastGame LastGame;
	std::string ContribCount;
	std::string ContribYield;
	std::string TotalPoints;
	std::string TotalTruePoints;
	std::string TotalSoftcorePoints;
	std::string Permissions;
	std::string Untracked;
	std::string ID;
	std::string UserWallActive;
	std::string Motto;
	std::string Rank;
	std::string TotalRanked;	
	std::map<std::string, Award> Awarded;
	std::map<std::string, std::vector<RecentAchievement>> RecentAchievements;
	std::string Points;
	std::string UserPic;
	std::string Status;

	// The cached games and the account's points as the offline proxy holds
	// them, read on this device while offline (fork #180).
	bool FromDevice = false;
};

struct UserRankAndScore
{
	int Score;
	int SoftcoreScore;
	std::string Rank;
	int TotalRanked;
};

// toRetroAchivementInfo
struct RetroAchievementGame
{
	std::string id;
	std::string name;
	std::string consoleName;
	std::string achievements;		
	std::string lastplayed;
	std::string badge;

	int scoreSoftcore;
	int scoreHardcore;
	int possibleScore;

	int wonAchievementsSoftcore;
	int wonAchievementsHardcore;
	int totalAchievements;

	bool progressUnknown = false;   // Award::AchievedUnknown: what was earned could not be read
};

struct RetroAchievementInfo
{
	std::string username;
	std::string points;
	std::string totalpoints;
	std::string softpoints;
	std::string rank;
	std::string userpic;
	std::string registered;
	std::string error;
	std::vector<RetroAchievementGame> games;
	bool fromDevice = false;
};

class RetroAchievements
{
public:
	static std::string				getApiUrl(const std::string& method, const std::string& parameters);
	static std::string				getApiLogin();
	static std::string				getMissingLoginMessage();
	static std::string				getLoginErrorMessage(HttpReq& req);
	static UserSummary				getUserSummary(const std::string& userName = "", int gameCount = 100);
	// cheevosHash is the ROM's hash from the gamelist: the offline proxy
	// keys a game started once through RetroArch by it, and a game the
	// scan cached by its id, so both are asked (fork #180).
	static GameInfoAndUserProgress	getGameInfoAndUserProgress(int gameId, const std::string& userName = "", const std::string& cheevosHash = "");

	// The same two, answered from the offline proxy's cache on this device
	// (fork #180, D-RA-009). A game the proxy holds comes back with FromDevice
	// set; one it has never cached with NotOnDevice set; a proxy that does
	// not answer leaves ID at 0 with ProxyDidNotAnswer set, and the caller
	// asks the web. pending, when given, is the queued awards already read,
	// so a summary over many games runs the ctl once.
	//
	// Both block on the proxy -- a process, then a request or three per
	// game -- so they run from GuiLoading's worker (GuiRetroAchievements::show,
	// GuiGameAchievements::show) and never on the interface thread. The
	// summary asks the proxy once per cached game (no bulk answer exists on
	// the proxy's side) and stops at the first game the proxy does not
	// answer for: one timeout, not one per game (audit #186 PL-09).
	static GameInfoAndUserProgress	getGameInfoFromDevice(int gameId, const std::string& cheevosHash, const std::vector<OfflineAchievementsText::PendingAward>* pending = nullptr);
	static UserSummary				getUserSummaryFromDevice();
	static UserRankAndScore         getUserRankAndScore(const std::string& userName);

	static RetroAchievementInfo		toRetroAchivementInfo(UserSummary& ret);

	static std::map<std::string, std::string>	getCheevosHashes();

	static std::string				getCheevosHash(SystemData* pSystem, const std::string& fileName);
	static bool						testAccount(const std::string& username, const std::string& password, std::string& tokenOrError, bool* refused = nullptr);

private:
	static std::string				getCheevosHashFromFile(int consoleId, const std::string& fileName);
};
