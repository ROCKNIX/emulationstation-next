#include "MultiLineMenuEntry.h"
#include "Window.h"
#include "components/TextComponent.h"
#include "components/ComponentGrid.h"
#include "components/ComponentList.h"
#include "math/Vector2i.h"
#include "math/Vector2f.h"
#include "ThemeData.h"
#include "utils/HtmlColor.h"

#define SUBSTRING_OPACITY	192

MultiLineMenuEntry::MultiLineMenuEntry(Window* window, const std::string& text, const std::string& substring, bool multiLine) :
	ComponentGrid(window, Vector2i(1, 2))
{
	mMultiLine = multiLine;
	mSizeChanging = false;
	mDimmed = false;

	auto theme = ThemeData::getMenuTheme();
	mColor = theme->Text.color;

	mText = std::make_shared<TextComponent>(mWindow, text.c_str(), theme->Text.font, theme->Text.color);
	mText->setMultiLine(TextComponent::MultiLineType::SINGLELINE);
	mText->setVerticalAlignment(ALIGN_TOP);

	mSubstring = std::make_shared<TextComponent>(mWindow, substring.c_str(), theme->TextSmall.font, Utils::HtmlColor::applyColorOpacity(theme->Text.color, SUBSTRING_OPACITY));
	mSubstring->setVerticalAlignment(ALIGN_TOP);

	if (!multiLine)
		mSubstring->setMultiLine(TextComponent::MultiLineType::SINGLELINE);

	setEntry(mText, Vector2i(0, 0), true, true);
	setEntry(mSubstring, Vector2i(0, 1), false, true);

	layoutRows();
}

// The two rows from the texts' natural heights, which for a single-line
// text is its font's height plus its padding. Not from the components'
// current sizes: the grid hands each text its cell -- 0.9 and 1.1 of the
// natural heights, below -- and TextComponent stops sizing itself once it
// has been given a height, so reading the cells back here made every
// setDescription shrink the label and grow the line by ten percent. A row
// refreshed once looked a little tight; one refreshed per game of a
// 37-game scan was a screen tall with the two texts drawn on top of each
// other (fork #241, 2026-09-22). The multi-line substring keeps its own
// measured height: onSizeChanged gives it a zero height first, so the
// value read is fresh.
static float naturalHeight(const std::shared_ptr<TextComponent>& text)
{
	if (text->getFont() == nullptr)
		return text->getSize().y();
	const Vector4f padding = text->getPadding();
	return text->getFont()->getHeight() + padding.y() + padding.w();
}

void MultiLineMenuEntry::layoutRows()
{
	float th = naturalHeight(mText);

	if (mSubstring->getText().empty())
	{
		setRowHeight(0, th);
		setRowHeight(1, 0);

		setSize(Vector2f(mSize.x(), th));
	}
	else
	{
		float sh = mMultiLine ? mSubstring->getSize().y() : naturalHeight(mSubstring);
		float h = th + sh;

		setRowHeightPerc(0, (th * 0.9) / h);
		setRowHeightPerc(1, (sh * 1.1) / h);

		setSize(Vector2f(mSize.x(), h));
	}
}

void MultiLineMenuEntry::setColor(unsigned int color)
{
	mColor = color;
	const unsigned int shown = mDimmed ? ComponentListFlags::dimmed(color) : color;
	mText->setColor(shown);
	mSubstring->setColor(Utils::HtmlColor::applyColorOpacity(shown, SUBSTRING_OPACITY));
}

void MultiLineMenuEntry::setDimmed(bool dimmed)
{
	mDimmed = dimmed;
	setColor(mColor);
}

// A ComponentGrid draws nothing of its own, so padding set on the entry has
// to reach the two text components inside it. Without this, an entry placed
// in a non-selectable row sits flush against the left edge while selectable
// rows and group headers are inset by ComponentList (which only offsets
// selectable rows -- see TOTAL_HORIZONTAL_PADDING_PX there).
void MultiLineMenuEntry::setPadding(const Vector4f padding)
{
	GuiComponent::setPadding(padding);
	mText->setPadding(padding);
	mSubstring->setPadding(padding);
}

std::string MultiLineMenuEntry::getDescription()
{
	return mSubstring->getText();
}

void MultiLineMenuEntry::setDescription(const std::string& description)
{
	mSubstring->setText(description);
	if (mMultiLine)
		onSizeChanged();
	else
		layoutRows();
}

void MultiLineMenuEntry::onSizeChanged()
{		
	ComponentGrid::onSizeChanged();

	if (mMultiLine && mSubstring && mSize.x() > 0 && !mSizeChanging)
	{
		mSizeChanging = true;

		mText->setSize(mSize.x(), 0);
		mSubstring->setSize(mSize.x(), 0);

		layoutRows();

		mSizeChanging = false;
	}
}

void MultiLineMenuEntry::onFocusGained()
{
	ComponentGrid::onFocusGained();

	if (!mMultiLine && mSubstring)
		mSubstring->setAutoScroll(TextComponent::AutoScrollType::HORIZONTAL);

}

void MultiLineMenuEntry::onFocusLost()
{
	ComponentGrid::onFocusLost();

	if (!mMultiLine && mSubstring)
		mSubstring->setAutoScroll(TextComponent::AutoScrollType::NONE);
}
