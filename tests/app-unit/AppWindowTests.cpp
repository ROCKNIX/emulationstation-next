// A worker's post to the window after main() has let it go (#308
// 8-es-menus-and-core claude F-ES-26): nothing is posted once the window is
// closing, and closing waits for a post in progress to finish.
#include "doctest/doctest.h"
#include "AppWindow.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

namespace
{
	struct FakeWindow
	{
		std::atomic<int> posts{ 0 };
		std::atomic<bool> gone{ false };
		std::atomic<int> postsAfterGone{ 0 };
		void postToUiThread(const std::function<void()>&)
		{
			if (gone)
				postsAfterGone++;
			// A post that takes a while, as Window's does under its lock.
			std::this_thread::sleep_for(std::chrono::microseconds(200));
			posts++;
		}
	};
}

TEST_CASE("app window: no post reaches a window once it is closing")
{
	FakeWindow w;
	std::atomic<bool> stop{ false };
	std::thread worker([&] { while (!stop) AppWindow::post(&w, [] {}); });
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	CHECK(w.posts > 0);

	AppWindow::closing();   // main(), before the window is torn down
	w.gone = true;          // from here the window is freed memory
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	stop = true;
	worker.join();

	CHECK(w.postsAfterGone == 0);
	CHECK_FALSE(AppWindow::post(&w, [] {}));
}
