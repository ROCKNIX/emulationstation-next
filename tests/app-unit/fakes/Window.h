#pragma once
// Double for es-core/src/Window.h. A posted task runs at once on the
// posting thread: the code under test never depends on which thread runs
// it, only on whether it runs.
#include <functional>
class AsyncNotificationComponent;
class Window
{
public:
	void postToUiThread(const std::function<void()>& func, void* data = nullptr);
	AsyncNotificationComponent* createAsyncNotificationComponent(bool actionLine = false);
};
