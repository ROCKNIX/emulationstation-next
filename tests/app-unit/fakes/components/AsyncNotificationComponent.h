#pragma once
// Double for es-core/src/components/AsyncNotificationComponent.h: a card
// that records what it was told, for the tests to read.
#include <atomic>
#include <mutex>
#include <string>
#include <vector>
class AsyncNotificationComponent
{
public:
	void updateTitle(const std::string text);
	void updateText(const std::string text, const std::string action = "");
	void updateText(const std::string text, const std::vector<std::string>& actionCandidates);
	void updateText(const std::vector<std::string>& textCandidates, const std::vector<std::string>& actionCandidates);
	void updatePercent(int percent);
	void close();

	std::mutex lock;
	std::string title;
	std::vector<std::string> text;
	std::vector<std::string> action;
	int percent = 0;
	std::atomic<bool> closed{ false };
};
