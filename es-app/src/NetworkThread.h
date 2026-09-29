#pragma once

#include "Window.h"
#include <thread>
#include <condition_variable>
#include <mutex>
#include <list>

#include "InputManager.h"
#include "ApiSystem.h"
#include "CheevosRetry.h"
#include "watchers/WatchersManager.h"

class CheckPadsBatteryLevelComponent : public IWatcher
{
public:
	CheckPadsBatteryLevelComponent();

	std::vector<PadInfo>& getPadsInfo() { return mPadsInfo; }

protected:
	bool enabled() override { return mEnabled; };
	int  updateTime() override { return 15 * 1000; } // 15 seconds

	bool check() override;

private:
	std::vector<PadInfo> mPadsInfo;
	bool mEnabled;
};

class CheckUpdatesComponent : public IWatcher
{
public:
	CheckUpdatesComponent() { mEnabled = true; }

	std::string& getLastUpdateMessage() { return mLastUpdateMessage; }

protected:
	bool enabled() override;
	int  updateTime() override { return 24 * 60 * 60 * 1000; } // 24 * 60 minutes
	bool check() override;

private:
	std::string mLastUpdateMessage;
	bool mEnabled;
};

class CheckCheevosTokenComponent : public IWatcher
{
public:
	std::string& getLastToken() { return mLastToken; }

	// The last check could not reach RetroAchievements, as against being
	// turned down by it: worth running again as soon as the network is up
	// (#175), rather than at the next scheduled check two hours on.
	bool retryWhenOnline() const { return mRetryWhenOnline; }

	// NetworkThread's view of the link, told on the watchers' thread -- the
	// one check() runs on -- whenever it changes. A link that has just come
	// up opens a new window of short retries (#175, CheevosRetry.h).
	void setOnline(bool online);

protected:
	bool enabled() override;
	// Decided by the last check(): CheevosRetry::ScheduledMs (120 minutes)
	// unless that check could not reach the server while the network was
	// up, when it is a short retry, a bounded number of times.
	int  updateTime() override { return mNextDelayMs; }
	int  initialUpdateTime() override { return 100; } // 100ms
	bool check() override;

private:
	std::string mLastToken;
	bool mRetryWhenOnline = false;
	bool mOnline = false;
	int  mUnreachableInARow = 0;
	int  mNextDelayMs = CheevosRetry::ScheduledMs;
};

class NetworkStateWatcher;

class NetworkThread : public IJoystickChangedEvent, public IWatcherNotify
{
public:
	NetworkThread(Window * window);

	// Ask the RetroAchievements token check to run now, on the watchers'
	// thread, rather than at its next scheduled time: the settings page's
	// close, which used to sign in on the interface thread itself (#308
	// 1-raoffline claude F-RA-19). Called on the interface thread, as the
	// link-up's own reset is. Nothing before the thread exists.
	static void checkCheevosTokenSoon();

public:
	void onJoystickChanged() override;

private:
	void OnWatcherChanged(IWatcher* component) override;

	CheckPadsBatteryLevelComponent					mCheckPadsBatteryLevelComponent;
	CheckUpdatesComponent							mCheckUpdatesComponent;
	CheckCheevosTokenComponent						mCheckCheevosTokenComponent;
	NetworkStateWatcher*							mNetworkStateWatcher;
	Window* mWindow;
};


