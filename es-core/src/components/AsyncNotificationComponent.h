#pragma once

#include <mutex>
#include "GuiComponent.h"
#include <string>
#include <vector>

class ComponentGrid;
class NinePatchComponent;
class TextComponent;
class Window;

class AsyncNotificationComponent : public GuiComponent
{
	friend class Window;

protected:
	AsyncNotificationComponent(Window* window, bool actionLine = true);
	~AsyncNotificationComponent();

public:
	void update(int deltaTime) override;
	void render(const Transform4x4f& parentTrans) override;

	void updateTitle(const std::string text);
	void updateText(const std::string text, const std::string action = "");
	// The action row from a list of candidates, longest first: the first
	// that fits the row's width is the one shown, else the last. Measured
	// in render(), on the interface thread, with the row's own font --
	// callers on a worker thread have no safe way to size text, and the
	// fonts' glyph atlas is a GL resource. Written for the cloud sync
	// card's outcome line (D-UI-028): what is in place plus how to recover
	// where both fit, the recovery alone where they do not.
	void updateText(const std::string text, const std::vector<std::string>& actionCandidates);
	// Both rows from candidates. The text row is composed too -- the
	// outcome word and then a whole sentence saying why -- so it runs off
	// a 640x480 panel and was clipped mid-word with an ellipsis, which is
	// the one thing a reason line must not do (#115). Same rule as the
	// action row: longest first, first that fits.
	void updateText(const std::vector<std::string>& textCandidates, const std::vector<std::string>& actionCandidates);
	void updatePercent(int percent);

	float getFading() { return mFadeOut; }
	bool isClosing() { return mClosing; };
	bool isRunning() { return mRunning; };
	Vector2f getFullSize() { return mFullSize; };

	void close();
	
private:
	std::shared_ptr<TextComponent> mTitle;
	std::shared_ptr<TextComponent> mGameName;
	std::shared_ptr<TextComponent> mAction;

	std::vector<std::string> mNextGameName; // candidates, longest first
	std::string mAppliedGameName;           // the candidates the row was last chosen from, joined
	std::string mNextTitle;
	std::vector<std::string> mNextAction;   // candidates, longest first
	std::string mAppliedAction;             // the candidates the row was last chosen from, joined

	// The first candidate that fits the row, else the last one offered.
	static std::string chooseThatFits(const std::shared_ptr<TextComponent>& row, const std::vector<std::string>& candidates);

	ComponentGrid* mGrid;
	NinePatchComponent* mFrame;

	std::mutex					mMutex;

	int mPercent;

	bool                mRunning;
	bool				mClosing;
	float				mFadeOut;
	int                 mFadeTime;
	int                 mFadeOutOpacity;

	Vector2f			mFullSize;
};