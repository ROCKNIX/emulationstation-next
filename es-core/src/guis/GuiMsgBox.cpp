#include "guis/GuiMsgBox.h"

#include "components/ButtonComponent.h"
#include "components/MenuComponent.h"
#include "components/ImageComponent.h"
#include "resources/ResourceManager.h"
#include "LocaleES.h"
#include "TextToSpeech.h"

#define HORIZONTAL_PADDING_PX  (Renderer::getScreenWidth()*0.01)

// A dialog is 0.6 of the screen wide, and 0.8 when it has a paragraph to
// say. The switch is measured, never matched on a string: a message that
// would wrap past MSGBOX_WIDE_LINES lines at the standard width is laid out
// at the wide one, so a confirmation that has to say several things -- what
// goes, what arrives, what is never touched -- reads as lines and not as a
// block (maintainer, 2026-09-07, the match preview on a 640-wide panel). A
// short message never widens; the box still shrinks to the text's own width
// when that is narrower than either cap. 0.8 is under the 0.9 that menus and
// the toast use, so a dialog still reads as a dialog over the page behind it.
#define MSGBOX_WIDTH           (Renderer::getScreenWidth() * 0.6f)
#define MSGBOX_WIDE_WIDTH      (Renderer::getScreenWidth() * 0.8f)
#define MSGBOX_WIDE_LINES      4

GuiMsgBox::GuiMsgBox(Window* window, const std::string& text, const std::string& name1, const std::function<void()>& func1, GuiMsgBoxIcon icon) 
	: GuiMsgBox(window, text, name1, func1, "", nullptr, "", nullptr, icon) { }

GuiMsgBox::GuiMsgBox(Window* window, const std::string& text,
	const std::string& name1, const std::function<void()>& func1,
	const std::string& name2, const std::function<void()>& func2,
	GuiMsgBoxIcon icon)
	: GuiMsgBox(window, text, name1, func1, name2, func2, "", nullptr, icon) { }

GuiMsgBox::GuiMsgBox(Window* window, const std::string& text, 
	const std::string& name1, const std::function<void()>& func1,
	const std::string& name2, const std::function<void()>& func2, 
	const std::string& name3, const std::function<void()>& func3,
	GuiMsgBoxIcon icon) : GuiComponent(window),
	mBackground(window, ":/frame.png"), mGrid(window, Vector2i(2, 2))
	
{
	auto theme = ThemeData::getMenuTheme();

	TextToSpeech::getInstance()->say(text);

	mBackground.setImagePath(theme->Background.path);
	mBackground.setEdgeColor(theme->Background.color);
	mBackground.setCenterColor(theme->Background.centerColor);
	mBackground.setCornerSize(theme->Background.cornerSize);
	mBackground.setPostProcessShader(theme->Background.menuShader);

	float width = MSGBOX_WIDTH; // max width
	float minWidth = Renderer::getScreenWidth() * 0.3f; // minimum width
	
	mImage = nullptr;

	std::string imageFile;

	switch (icon)
	{
	case ICON_INFORMATION:
		imageFile = ":/info.svg";
		break;
	case ICON_QUESTION:
		imageFile = ":/question.svg";
		break;
	case ICON_WARNING:
		imageFile = ":/warning.svg";
		break;
	case ICON_ERROR:
		imageFile = ":/alert.svg";
		break;
	case ICON_AUTOMATIC:

		if (text.rfind("?") != std::string::npos || name1 == _("YES"))
			imageFile = ":/question.svg";
		else if (name1 == _("OK"))
		{
			if (name2.empty())
				imageFile = ":/info.svg";
			else
				imageFile = ":/question.svg";
		}

		break;
	}

  // ensure the tailscale message doesn't wrap, and no icon (for more room)
	if (text.find("tailscale.com") != std::string::npos)
	{
			width = Renderer::getScreenWidth() * 0.9f; // max width
	} else if (!imageFile.empty() && ResourceManager::getInstance()->fileExists(imageFile) && !Renderer::isSmallScreen())
	{
		mImage = std::make_shared<ImageComponent>(window);
		mImage->setImage(imageFile);
		mImage->setIsLinear(true);
		mImage->setColorShift(theme->Text.color);
		mImage->setOrigin(0.5f, 0.5f);
		mImage->setMaxSize(theme->Text.font->getLetterHeight() * 2.0f, theme->Text.font->getLetterHeight() * 2.0f);		

		mGrid.setEntry(mImage, Vector2i(0, 0), false, false);
	}

	mMsg = std::make_shared<TextComponent>(mWindow, text, ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color, mImage == nullptr || Renderer::isSmallScreen() ? ALIGN_CENTER : ALIGN_LEFT); // CENTER
	mMsg->setPadding(Vector4f(Renderer::getScreenWidth()*0.015f, 0, Renderer::getScreenWidth()*0.015f, 0));
	
	mGrid.setEntry(mMsg, Vector2i(mImage == nullptr ? 0 : 1, 0), false, false, Vector2i(mImage == nullptr ? 2 : 1, 1));

	// create the buttons
	mButtons.push_back(std::make_shared<ButtonComponent>(mWindow, name1, name1, std::bind(&GuiMsgBox::deleteMeAndCall, this, func1)));
	if(!name2.empty())
		mButtons.push_back(std::make_shared<ButtonComponent>(mWindow, name2, name2, std::bind(&GuiMsgBox::deleteMeAndCall, this, func2)));
	if(!name3.empty())
		mButtons.push_back(std::make_shared<ButtonComponent>(mWindow, name3, name3, std::bind(&GuiMsgBox::deleteMeAndCall, this, func3)));

	// set accelerator automatically (button to press when BUTTON_BACK is pressed)
	if(mButtons.size() == 1)
	{
		mAcceleratorFunc = mButtons.front()->getPressedFunc();
	}
	else if (mButtons.size() > 0)
	{
		for(auto it = mButtons.cbegin(); it != mButtons.cend(); it++)
		{
			if(Utils::String::toUpper((*it)->getText()) == _("OK") || Utils::String::toUpper((*it)->getText()) == _("NO"))
			{
				mAcceleratorFunc = (*it)->getPressedFunc();
				break;
			}
		}

		if (mAcceleratorFunc == nullptr)
			mAcceleratorFunc = mButtons.back()->getPressedFunc();
	}

	// put the buttons into a ComponentGrid
	mButtonGrid = makeButtonGrid(mWindow, mButtons);
	mGrid.setEntry(mButtonGrid, Vector2i(0, 1), true, false, Vector2i(2, 1), GridFlags::BORDER_TOP);

	// A paragraph gets the wide box (see MSGBOX_WIDE_LINES). Measured with
	// the text's own font at the width the text would actually get.
	if (width < MSGBOX_WIDE_WIDTH)
	{
		float textWidth = width - 3 * HORIZONTAL_PADDING_PX;
		if (mImage != nullptr)
			textWidth -= mImage->getSize().x() + 2 * HORIZONTAL_PADDING_PX;
		const float lineHeight = mMsg->getFont()->getHeight();
		if (lineHeight > 0 && mMsg->getFont()->sizeWrappedText(text, textWidth).y() > lineHeight * MSGBOX_WIDE_LINES)
			width = MSGBOX_WIDE_WIDTH;
	}

	// decide final width
	if(mMsg->getSize().x() < width && mButtonGrid->getSize().x() < width)
	{
		// mMsg and buttons are narrower than width
		width = Math::max(mButtonGrid->getSize().x(), mMsg->getSize().x() + 3 * HORIZONTAL_PADDING_PX);

		if (mImage != nullptr)
			width += mImage->getSize().x() + 2 * HORIZONTAL_PADDING_PX;

		width = Math::max(width, minWidth);
	}
	
	// now that we know width, we can find height
	mMsg->setSize(width, 0); // mMsg->getSize.y() now returns the proper length

	// ...except that it does not, and on a small panel the difference puts
	// the OK button on top of the message (#48).
	//
	// A TextComponent measures its automatic height by wrapping at its full
	// width -- onTextChanged() calls sizeWrappedText(text, getSize().x()) --
	// and draws by wrapping at its width minus its own horizontal padding:
	// buildTextCache() lays the glyphs out at sx = mSize.x() - mPadding.x()
	// - mPadding.z(). mMsg carries 0.015 of the screen on each side, so on a
	// 640-wide panel the glyphs wrap at 364.8px in a box measured at 384 --
	// 5% narrower than the height was measured for, 3.75% in the wide box --
	// and the drawn text gains a line whenever a wrap point falls in that
	// band.
	//
	// Nothing catches the extra line: a TextComponent with no autoscroll
	// pushes no clip rect, so it paints straight through whatever is under
	// it, which here is the button row.
	//
	// The 1.225 below is what hid it. It buys 0.225 of a line per line, so
	// a message of five drawn lines or more absorbs one extra line and a
	// shorter one does not -- and how many lines a message has depends on
	// the panel, since the box is a fraction of the screen and the font is
	// not scaled with it in the same proportion. That is why the same
	// dialog is fine on a 1080p VM and has its button sitting on the text
	// of an RG35XX SP. The bug is not the small screen; it is that the
	// dialog measured one thing and drew another.
	//
	// So measure the height at the width the glyphs are actually laid out
	// at. sizeWrappedText is sizeText(wrapText(text, xLen)) and
	// buildTextCache is buildTextCache(wrapText(text, sx)), so the two agree
	// exactly once they are given the same xLen. This is also the width the
	// wide-box test above already measures at.
	const Vector4f msgPadding = mMsg->getPadding();
	const float drawnWidth = width - msgPadding.x() - msgPadding.z();
	float drawnHeight = mMsg->getSize().y();
	if (mMsg->getFont() != nullptr && drawnWidth > 0)
		drawnHeight = mMsg->getFont()->sizeWrappedText(text, drawnWidth).y() + msgPadding.y() + msgPadding.w();

	float msgHeight = Math::max(Font::get(FONT_SIZE_LARGE)->getHeight(), drawnHeight*1.225f);
	
	if (msgHeight + mButtonGrid->getSize().y() > Renderer::getScreenHeight())
	{
		// The message alone is taller than the screen. onSizeChanged() does
		// constrain mMsg to the grid row left above the buttons, but a
		// TextComponent renders at its natural height regardless of that
		// bound unless it is scrolling -- so the text painted straight
		// through the button row and the buttons appeared on top of it.
		// Seen on an RG35XX SP with a two-paragraph message; a
		// desktop-resolution VM never reaches this branch, which is why it
		// survived to hardware.
		//
		// Set before setSize so the flag is live when onSizeChanged() runs.
		// The grid still owns the geometry; this only makes the text respect it.
		mMsg->setAutoScroll(TextComponent::AutoScrollType::VERTICAL);

		setSize(Renderer::getScreenWidth(), Renderer::getScreenHeight());
		if (mImage != nullptr)
			mMsg->setSize(Renderer::getScreenWidth() - mImage->getSize().x() - 4* HORIZONTAL_PADDING_PX, 0);
	}
	else
		setSize(width + HORIZONTAL_PADDING_PX*2, msgHeight + mButtonGrid->getSize().y());

	// center for good measure
	setPosition((Renderer::getScreenWidth() - mSize.x()) / 2.0f, (Renderer::getScreenHeight() - mSize.y()) / 2.0f);

	addChild(&mBackground);
	addChild(&mGrid);

	TextToSpeech::getInstance()->say(text);

	for (auto btn : mButtons)
	{
		if (btn->hasFocus())
		{
			TextToSpeech::getInstance()->say(btn->getText(), true);
			break;
		}
	}
}

bool GuiMsgBox::input(InputConfig* config, Input input)
{
	// special case for when GuiMsgBox comes up to report errors before anything has been configured
	if(config->getDeviceId() == DEVICE_KEYBOARD && !config->isConfigured() && input.value && 
		(input.id == SDLK_RETURN || input.id == SDLK_ESCAPE || input.id == SDLK_SPACE))
	{
		mAcceleratorFunc();
		return true;
	}

	/* when it's not configured, allow to remove the message box too to allow the configdevice window a chance */
	if(mAcceleratorFunc && ((config->isMappedTo(BUTTON_BACK, input) && input.value != 0) || (config->isConfigured() == false && input.type == TYPE_BUTTON))) 
	{
		mAcceleratorFunc();
		return true;
	}

	return GuiComponent::input(config, input);
}

void GuiMsgBox::onSizeChanged()
{
	GuiComponent::onSizeChanged();

	mGrid.setSize(mSize);

	if (mImage != nullptr)
	{
		auto width = mImage->getSize().x() + (Renderer::isSmallScreen() ? 5 : 2) * HORIZONTAL_PADDING_PX;
		mGrid.setColWidthPerc(0, width / mSize.x(), true);
	}

	mGrid.setRowHeightPerc(1, mButtonGrid->getSize().y() / mSize.y());
			
	mMsg->setSize(mSize.x() - HORIZONTAL_PADDING_PX*2, mGrid.getRowHeight(0));
	mGrid.onSizeChanged();

	mBackground.fitTo(mSize, Vector3f::Zero(), Vector2f(-32, -32));
}

void GuiMsgBox::deleteMeAndCall(const std::function<void()>& func)
{
	auto funcCopy = func;
	delete this;

	if(funcCopy)
		funcCopy();

}

std::vector<HelpPrompt> GuiMsgBox::getHelpPrompts()
{
	return mGrid.getHelpPrompts();
}
