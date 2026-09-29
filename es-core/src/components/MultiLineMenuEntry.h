#pragma once

#include <string>
#include "components/ComponentGrid.h"

class TextComponent;
class Window;

class MultiLineMenuEntry : public ComponentGrid
{
public:
	MultiLineMenuEntry(Window* window, const std::string& text, const std::string& substring, bool multiLine = false);

	void setColor(unsigned int color) override;
	// A dimmed entry stays dimmed: the dim is applied on every setColor,
	// which ComponentList calls each frame (ComponentListFlags::dimmed,
	// fork #182).
	void setDimmed(bool dimmed);
	bool isDimmed() const { return mDimmed; }
	void setPadding(const Vector4f padding) override;
	void onSizeChanged() override;
	void onFocusGained() override;
	void onFocusLost() override;
	
	std::string getDescription();
	// A line set later re-lays the entry: an entry built with no line is
	// one line high and grows when one arrives, and shrinks when it goes
	// (the WI-FI NETWORK row, whose line is asked off the interface thread and
	// is often nothing -- fork #191). The list holding the row still has to
	// be re-laid by the caller (GuiSettings::updateSize).
	void setDescription(const std::string& description);

protected:
	// The two rows' heights from the two texts: the label alone when the
	// line under it is empty, both otherwise. The constructor, a later
	// setDescription and the multi-line relayout all go through here.
	void layoutRows();

	bool mMultiLine;
	bool mSizeChanging;
	bool mDimmed;
	// The colour the row last gave us, so setDimmed can re-apply it.
	unsigned int mColor;
	std::shared_ptr<TextComponent> mText;
	std::shared_ptr<TextComponent> mSubstring;
};
