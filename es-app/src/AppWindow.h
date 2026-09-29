#pragma once
#ifndef ES_APP_APP_WINDOW_H
#define ES_APP_APP_WINDOW_H

// A worker thread's post to the interface thread, and main()'s letting go of
// the window (#308 8-es-menus-and-core claude F-ES-26).
//
// The window is a local of main(). Detached workers -- the network page's
// address and SSID readers, the cloud hub's tidy-folders check, the exit's
// capture, the offline achievements' cards -- post to it when their script
// ends, and nothing joins them: one that ends after main() has torn the
// window down posts into freed memory, a mutex and a vector in a dead
// frame. So a worker posts through here, and main() calls closing() before
// the teardown. The lock is held across the post, so closing() waits for
// one in progress and no post begins after it: the check and the post are
// one step, not two with a gap between.
//
// Templated on the window so the rule has a test without SDL.
#include <functional>
#include <mutex>

namespace AppWindow
{
	inline std::mutex& gate()
	{
		static std::mutex m;
		return m;
	}

	inline bool& open()
	{
		static bool isOpen = true;
		return isOpen;
	}

	// Posts `task` unless the window is closing; false when it was not posted.
	template <class W>
	bool post(W* window, const std::function<void()>& task)
	{
		std::lock_guard<std::mutex> lock(gate());
		if (!open() || window == nullptr)
			return false;
		window->postToUiThread(task);
		return true;
	}

	// main(), once its loop has ended and before the window goes: every post
	// from here on is dropped.
	inline void closing()
	{
		std::lock_guard<std::mutex> lock(gate());
		open() = false;
	}
}

#endif // ES_APP_APP_WINDOW_H
