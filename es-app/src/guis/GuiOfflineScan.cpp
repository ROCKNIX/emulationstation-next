#include "guis/GuiOfflineScan.h"
#include "TextFit.h"

#include "CloudExit.h"
#include "guis/GuiMsgBox.h"
#include "CloudText.h"
#include "OfflineAchievements.h"
#include "Window.h"
#include "ThemeData.h"
#include "LocaleES.h"
#include "utils/StringUtil.h"
#include "Log.h"

#include <cstdio>

GuiOfflineScan::GuiOfflineScan(Window* window, const std::string& command, const std::function<void()>& onChanged)
	: GuiComponent(window), mBusyAnim(window, ""), mBackground(window, ":/frame.png"),
	  mCommand(command), mOnChanged(onChanged), mShownFinished(false)
{
	auto theme = ThemeData::getMenuTheme();
	mBackground.setImagePath(theme->Background.path);
	mBackground.setEdgeColor(theme->Background.color);
	mBackground.setCenterColor(theme->Background.centerColor);
	mBackground.setCornerSize(theme->Background.cornerSize);

	const float SW = Renderer::getScreenWidth();
	const float SH = Renderer::getScreenHeight();
	setSize(SW, SH);

	mTextFont  = theme->Text.font;
	mSmallFont = theme->TextSmall.font;
	mTitle    = std::make_shared<TextComponent>(window, _("SCANNING GAMES FOR OFFLINE ACHIEVEMENTS"), theme->Title.font, theme->Title.color, ALIGN_CENTER);
	mStatus   = std::make_shared<TextComponent>(window, _("PREPARING..."), mTextFont,  theme->Text.color,      ALIGN_CENTER);
	mCounter  = std::make_shared<TextComponent>(window, "",                mSmallFont, theme->TextSmall.color, ALIGN_CENTER);
	mActivity = std::make_shared<TextComponent>(window, "",                mTextFont,  theme->Text.color,      ALIGN_CENTER);
	mDetail   = std::make_shared<TextComponent>(window, "",                mSmallFont, theme->TextSmall.color, ALIGN_CENTER);
	mNote     = std::make_shared<TextComponent>(window, "",                mSmallFont, theme->TextSmall.color, ALIGN_CENTER);
	mElapsed  = std::make_shared<TextComponent>(window, "",                mSmallFont, theme->TextSmall.color, ALIGN_CENTER);
	mFooter   = std::make_shared<TextComponent>(window, "",                mSmallFont, theme->TextSmall.color, ALIGN_CENTER);

	// One column, 0.78 of the width, every line fitted to it, and the rows
	// stacked from the fonts' own heights -- GuiCloudTransfer's geometry,
	// so the two long-job pages of this app sit the same way on every panel
	// (its constructor carries the derivation and the numbers per panel).
	const float w  = SW * 0.78f;
	const float cx = SW * 0.5f;
	const float x  = cx - w / 2.0f;
	mLineWidth = w;
	for (auto& t : { mTitle, mStatus, mCounter, mActivity, mDetail, mNote, mElapsed, mFooter })
		t->setSize(w, 0);

	const float hT = theme->Title.font->getHeight();
	const float hM = mTextFont->getHeight();
	const float hS = mSmallFont->getHeight();
	const float natural = 2 * SH * 0.05f + hT + 3 * hM + 4 * hS + 5.5f * SH * 0.02f;
	const float fit = natural > SH * 0.9f ? (SH * 0.9f) / natural : 1.0f;
	const float pad = SH * 0.05f * fit;
	const float gap = SH * 0.02f * fit;
	const float rT = hT * fit, rM = hM * fit, rS = hS * fit;
	const float H = 2 * pad + rT + 3 * rM + 4 * rS + 5.5f * gap;
	const float top = (SH - H) / 2.0f;

	float y = top + pad;
	mTitle   ->setPosition(x, y);
	y += rT + 1.5f * gap;
	mStatus  ->setPosition(x, y);
	mCounter ->setPosition(x, y + rM);
	y += rM + rS + gap;
	mActivity->setPosition(x, y);
	mDetail  ->setPosition(x, y + rM);
	y += rM + rS + 0.5f * gap;
	// 5. The spinner while it runs, the note once done, one row. No caption
	// on the spinner: line 1 says what is happening, and BusyComponent's
	// setText("") is a no-op against its empty start (es-code-traps.md).
	mBusyAnim.setBackgroundVisible(false);
	mBusyAnim.setSize(w, rM);
	mBusyAnim.setPosition(x, y);
	mNote    ->setPosition(x, y + (rM - hS) / 2.0f);
	y += rM + 1.5f * gap;
	mElapsed ->setPosition(x, y);
	y += rS + gap;
	mFooter  ->setPosition(x, y);

	mPanelSize = Vector2f(w + SW * 0.06f, H);
	mPanelPos  = Vector2f(cx - mPanelSize.x() / 2.0f, top);
	mBackground.fitTo(mPanelSize, Vector3f(mPanelPos.x(), mPanelPos.y(), 0), Vector2f(-32, -32));

	// The run in flight, when there is one, else a new one. Either way the
	// row follows it from here.
	mJob = OfflineScanJob::start(window, mCommand);
	mJob->setOnChanged(mOnChanged);
}

// Nothing to join: the run is the job's (OfflineScanJob). The page is sat
// in since D-UI-078 -- CANCEL ends the run before the page goes -- so the
// job outliving the page is the seam's guard, not the design.
GuiOfflineScan::~GuiOfflineScan()
{
}

// While the scan runs, back asks whether to cancel it (D-UI-078: the page is
// sat in, and CANCEL is the one way out); every other press is refused,
// since there is nothing to choose and a stray press should not dismiss a
// page somebody is waiting on. Once it has finished, any button dismisses
// it, and when the run did not complete, OK runs it again from here: the
// surface that reported the failure carries the retry (D-UI-028). OK and
// back are the player's mapping, which may be swapped; the literal "a" the
// retry was bound to was back's own button then (#308 1-raoffline claude
// F-RA-14, gpt F-RA-22).
bool GuiOfflineScan::input(InputConfig* config, Input input)
{
	if (!input.value)
		return true;
	const OfflineScanJob::State s = mJob->state();
	if (!s.finished)
	{
		if (config->isMappedTo(BUTTON_BACK, input))
			askCancel();
		return true;
	}
	const Outcome o = outcome(s);
	if (!o.completed && config->isMappedTo(BUTTON_OK, input))
	{
		// A new run; the finished one is let go. The page shows the run
		// from its first line again.
		mJob = OfflineScanJob::start(mWindow, mCommand);
		mJob->setOnChanged(mOnChanged);
		mShownFinished = false;
		mStatus->setText(_("PREPARING..."));
		for (auto& t : { mCounter, mActivity, mDetail, mNote })
			t->setText("");
		updateHelpPrompts();
		return true;
	}
	close();
	return true;
}

// What cancelling means, read at the moment of deciding (D-UI-023): the
// games already saved stay saved, and a scan passes over games already
// cached, so the next one carries on from here. YES first, NO last so B
// answers NO. The run is captured, not the page: the page outlives the
// dialog either way, but nothing here needs it.
void GuiOfflineScan::askCancel()
{
	std::shared_ptr<OfflineScanJob> job = mJob;
	mWindow->pushGui(new GuiMsgBox(mWindow,
		_("CANCEL THE SCAN?") + std::string("\n\n")
			+ _("GAMES ALREADY SAVED STAY SAVED. THE NEXT SCAN CARRIES ON FROM THERE."),
		_("YES"), [job] { job->cancel(); },
		_("NO"), nullptr));
}

void GuiOfflineScan::close()
{
	// Copied out first: the page is gone by the time it runs.
	std::function<void()> onChanged = mOnChanged;
	delete this;
	if (onChanged)
		onChanged();
}

std::vector<HelpPrompt> GuiOfflineScan::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	const OfflineScanJob::State s = mJob->state();
	if (!s.finished)
	{
		prompts.push_back(HelpPrompt(BUTTON_BACK, _("CANCEL")));
		return prompts;
	}
	const Outcome o = outcome(s);
	if (!o.completed)
		prompts.push_back(HelpPrompt(BUTTON_OK, _("TRY AGAIN")));
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("CLOSE")));
	return prompts;
}

// The word for the run (D-UI-028): COMPLETED, SKIPPED for the two sentinels
// the ctl exits with before touching anything -- not online, another scan
// or top-up holding the lock -- and for the player's own cancel (the word
// the transfer page gives a run stopped for a game, D-CLOUD-129: nothing
// went wrong, and the next run carries on), and COULDN'T FINISH for
// everything else, the refusals included: the why on line 4 says which.
GuiOfflineScan::Outcome GuiOfflineScan::outcome(const OfflineScanJob::State& s)
{
	Outcome o;
	o.completed = !s.cancelled && s.exit == 0;
	o.skipped = s.cancelled || s.exit == CloudExit::NoNetwork || s.exit == CloudExit::LockHeld;
	if (s.cancelled)
		o.word = _("SKIPPED - YOU CANCELLED IT");
	else if (o.completed)
		o.word = _("COMPLETED");
	else if (s.exit == CloudExit::NoNetwork)
		o.word = _("SKIPPED - YOU'RE NOT ONLINE");
	else if (s.exit == CloudExit::LockHeld)
		o.word = _("SKIPPED - A SCAN IS ALREADY RUNNING");
	else
		o.word = _("COULDN'T FINISH");
	return o;
}

void GuiOfflineScan::render(const Transform4x4f& parentTrans)
{
	Transform4x4f trans = parentTrans * getTransform();
	Renderer::setMatrix(trans);
	// Dim the whole screen: this is a takeover, and the carousel showing
	// through would say the app is idle while a scan is in flight.
	Renderer::drawRect(0.f, 0.f, mSize.x(), mSize.y(), 0x000000D0);

	mBackground.render(trans);

	for (auto& t : { mTitle, mStatus, mCounter, mActivity, mDetail, mNote, mElapsed, mFooter })
		t->render(trans);

	// A spinner and never a bar: the client reports each game as it
	// finishes it and nothing in between, and hashing a disc image can take
	// as long as ten cartridges, so a bar at i/n would be an invented
	// position (es-native-ui.md: a bar only where a real percentage exists).
	if (!mShownFinished)
		mBusyAnim.render(trans);

	// The keys are the help bar's, drawn here: with full-screen menus on
	// (every handheld) Window draws no help while a second page is open, so
	// the footer spelled them out as A and B -- the wrong buttons on a
	// swapped mapping (es-ui-style-guide.md, Interaction rules; the save
	// state manager draws its bar the same way).
	if (mWindow->peekGui() == this && Renderer::ScreenSettings::fullScreenMenus())
		mWindow->renderHelpPromptsEarly(parentTrans);
}

// Clip to one line: a long game name gets an ellipsis, never a second line.
std::string GuiOfflineScan::fitOneLine(const std::shared_ptr<Font>& font, std::string text, float width)
{
	if (!font)
		return text;
	// On characters, not bytes (TextFit; #308 gpt F-RA-23).
	return TextFit::fitOneLine(text, width, [&font](const std::string& t) { return font->sizeText(t).x(); });
}

// "N GAMES READY FOR OFFLINE PLAY": the one sentence the row under SCAN
// GAMES and this page's line 3 both end on, so they never disagree.
std::string GuiOfflineScan::readyPhrase(int ready)
{
	if (ready <= 0)
		return _("NO GAMES READY FOR OFFLINE PLAY YET");
	if (ready == 1)
		return std::string("1 ") + _("GAME READY FOR OFFLINE PLAY");
	return std::to_string(ready) + " " + std::string(_("GAMES READY FOR OFFLINE PLAY"));
}

// "GAMES WITH ACHIEVEMENTS ADDED: 3" -- what this run did, and "NOT SAVED: 1"
// beside it when a fetch failed for any game (the ctl's errors count, audit
// #186 PL-24: such a run is COULDN'T FINISH, and the next scan tries those
// games again). The games without a set are not counted here: the
// maintainer, on the RG SP with RC-5 (2026-09-14), "I'm not that concerned
// about the games that don't have achievements [...] I just want to know
// that it's scanning through the games with achievements." That count still
// travels in the stamp for the log (skipped), unused here.
std::string GuiOfflineScan::countsLine(int cached, int skipped, int errors)
{
	(void) skipped;
	std::string line = std::string(_("GAMES WITH ACHIEVEMENTS ADDED:")) + " " + std::to_string(cached);
	if (errors > 0)
		line += "  ·  " + std::string(_("NOT SAVED:")) + " " + std::to_string(errors);
	return line;
}

void GuiOfflineScan::update(int deltaTime)
{
	GuiComponent::update(deltaTime);
	mBusyAnim.update(deltaTime);
	const OfflineScanJob::State s = mJob->state();
	if (s.finished && !mShownFinished)
	{
		mShownFinished = true;
		// The help bar changes with the page: TRY AGAIN and CLOSE now.
		updateHelpPrompts();
	}
	const int mins = s.elapsedMs / 60000;
	const int secs = (s.elapsedMs / 1000) % 60;
	char elapsed[32];
	snprintf(elapsed, sizeof(elapsed), "%d:%02d", mins, secs);

	if (s.finished)
	{
		// The same seven rows, now carrying the outcome (D-UI-028): 1 the
		// word; 2 what this run added and passed over; 3 how many games
		// earn offline now -- the answer the page exists to give; 4 why it
		// stopped, when it did; 5 what to do next; 6 elapsed; 7 the buttons.
		const Outcome o = outcome(s);
		mStatus->setText(fitOneLine(mTextFont, o.word, mLineWidth));

		const bool ran = s.total > 0 || s.cached > 0 || s.skipped > 0 || s.nothingNew;
		mCounter->setText(ran && !s.nothingNew ? fitOneLine(mSmallFont, countsLine(s.cached, s.skipped, s.errors), mLineWidth) : "");

		// The ctl's done line carries the count; a run that ended before it
		// said one reads the client's export directly.
		const int ready = s.ready >= 0 ? s.ready : OfflineAchievements::readyCount();
		mActivity->setText(fitOneLine(mTextFont, readyPhrase(ready), mLineWidth));

		std::string detail;
		if (!o.completed && !o.skipped && !s.why.empty())
		{
			// The why whole, else its first sentence: a trailing instruction
			// is the part that can go (D-UI-035), never a word cut short.
			// SOME ACHIEVEMENT IMAGES COULDN'T BE SAVED. TRY THE SCAN AGAIN.
			// is 516 px of the theme's bold at 15 px against this line's 499
			// at 640x480, and the French of SOME GAMES... 521.
			const std::string why = OfflineAchievements::scanWhy(s.why);
			std::vector<std::string> forms = { why };
			const std::string first = CloudText::shortenWhy(why);
			if (!first.empty())
				forms.push_back(first);
			const auto font = mSmallFont;
			detail = CloudText::chooseThatFits(forms, mLineWidth, [font](const std::string& t) { return font->sizeText(t).x(); });
		}
		else if (!o.completed && !o.skipped)
			detail = _("SOMETHING WENT WRONG");
		mDetail->setText(fitOneLine(mSmallFont, detail, mLineWidth));

		// The client's cap on cached games is out of reach on ROCKNIX (patch
		// 004, D-RA-014), but the ctl still forwards the client's word when it
		// says it, so the branch stays -- with a sentence that names no
		// number, since the number is not this product's to promise
		// (audit #186 PL-15).
		std::string note;
		if (s.limit)
			note = _("THAT'S AS MANY GAMES AS CAN BE SAVED FOR OFFLINE PLAY.");
		else if (s.truncated)
			// The walk stopped at the client's cap of files per run (#186
			// PL-17): the games past it are the next run's.
			note = _("NOT EVERY FOLDER WAS LOOKED AT. SCAN AGAIN TO CONTINUE.");
		else if (s.nothingNew)
			note = _("NOTHING NEW - EVERY GAME WAS ALREADY READY.");
		else if (s.cancelled)
			note = _("THE NEXT SCAN CARRIES ON FROM HERE.");
		else if (s.exit == CloudExit::NoNetwork)
			note = _("TRY AGAIN WHEN YOU'RE ONLINE.");
		mNote->setText(fitOneLine(mSmallFont, note, mLineWidth));

		mElapsed->setText(std::string(_("ELAPSED")) + " " + elapsed);
		// A run that did not complete: TRY AGAIN and CLOSE are the help bar's,
		// on the player's own buttons.
		mFooter ->setText(!o.completed ? std::string() : _("PRESS ANY BUTTON TO CLOSE"));
	}
	else
	{
		// 1 what it is doing; 2 GAME i OF n; 3 the game it is on -- the line
		// that says it is alive, because a disc image can take a while and
		// a frozen count is indistinguishable from a hang; 4 the counts.
		if (s.index > 0)
			mStatus->setText(_("SCANNING..."));
		else if (s.listing)
			mStatus->setText(_("LOOKING THROUGH YOUR GAMES..."));
		else
			mStatus->setText(_("PREPARING..."));

		std::string counter;
		if (s.index > 0)
		{
			counter = std::string(_("GAME")) + " " + std::to_string(s.index);
			if (s.total > 0)
				counter += " " + std::string(_("OF")) + " " + std::to_string(s.total);
		}
		mCounter ->setText(counter);
		mActivity->setText(fitOneLine(mTextFont, s.game, mLineWidth));
		mDetail  ->setText(s.index > 0 ? fitOneLine(mSmallFont, countsLine(s.cached, s.skipped, s.errors), mLineWidth) : "");
		mNote    ->setText("");
		mElapsed ->setText(std::string(_("ELAPSED")) + " " + elapsed);
		// 7. That it takes a while; how to cancel is the help bar's, on the
		// player's own back button.
		mFooter  ->setText(fitOneLine(mSmallFont, _("THIS CAN TAKE A WHILE."), mLineWidth));
	}
}
