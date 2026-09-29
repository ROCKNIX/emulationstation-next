#include "CloudOffer.h"

#include "AppWindow.h"
#include "ApiSystem.h"
#include "Window.h"
#include "guis/GuiLoading.h"
#include "guis/GuiMenu.h"
#include "guis/GuiMsgBox.h"
#include "utils/StringUtil.h"
#include "LocaleES.h"

namespace CloudOffer
{

// The cloud answered, and there is no saves folder in it: not a failure,
// but it leaves the player with nothing restored and no obvious way
// forward (#100, D-CLOUD-085).
//
// When the parent holds a folder whose name is within a typo of the one we
// were told to read, the question changes (#127, D-CLOUD-091/092): creating
// it would put /ROCKNIX/Savez beside the real /ROCKNIX/Saves, after which
// every backup goes to the wrong folder and every restore finds nothing.
// So the first choice offered is to fix the name, and creating it anyway is
// the second.
static void createSavesFolder(Window* window, const std::vector<std::string>& args)
{
	// What the script found: the folder it was told to read, and -- when
	// there is one -- the folder beside it whose name is close to it.
	const std::string folder = args.size() > 0 ? args[0] : "";
	const std::string near = args.size() > 1 ? args[1] : "";

	auto create = [window]
	{
		window->pushGui(new GuiLoading<int>(window, _("SETTING UP YOUR CLOUD FOLDERS"),
			[](auto gui)
			{
				return ApiSystem::executeScriptLegacy("timeout 90 /usr/bin/cloud_setup --seed-folders",
					[](const std::string) {}).second;
			},
			[window](int rc)
			{
				window->pushGui(new GuiMsgBox(window, rc == 0
					? _("DONE. YOUR SAVES WILL GO THERE THE NEXT TIME YOU BACK THEM UP.")
					: _("COULDN'T CREATE IT. CHECK YOUR CONNECTION AND TRY AGAIN FROM MANAGE CLOUD STORAGE."),
					_("OK")));
			}));
	};

	if (!near.empty())
	{
		window->pushGui(new GuiMsgBox(window,
			Utils::String::format(_("NOTHING TO RESTORE: YOUR CLOUD HAS %s, NOT %s.\n\nIS THE NAME RIGHT?").c_str(),
				near.c_str(), folder.c_str()),
			_("CHANGE FOLDER"), [window, folder] { GuiMenu::openCloudFolderEditor(window, folder); },
			_("CREATE ANYWAY"), create,
			_("NOT NOW"), nullptr));
		return;
	}
	window->pushGui(new GuiMsgBox(window,
		folder.empty()
			? _("YOUR CLOUD HAS NO SAVES FOLDER YET.\n\nCREATE IT NOW?")
			: Utils::String::format(_("YOUR CLOUD HAS NO %s FOLDER YET.\n\nCREATE IT NOW?").c_str(), folder.c_str()),
		_("CREATE IT"), create,
		_("NOT NOW"), nullptr));
}

void present(Window* window, const std::string& offer, const std::vector<std::string>& args)
{
	if (window == nullptr || offer != "create-saves-folder")
		return;

	// The dialog is built on the interface thread wherever the answer came
	// from: the card reads the script on a worker, the page reads it on a
	// worker and asks from its own input handler, having just deleted
	// itself. Posting covers both -- and on the page it also puts the
	// dialog up on the frame after the one that took the press, rather than
	// inside the input dispatch of a component that is gone. Through
	// AppWindow: the card's worker is detached and asks after its linger,
	// which can be after main() has let the window go (#308 8-es claude
	// F-ES-26's rule).
	AppWindow::post(window, [window, args]() { createSavesFolder(window, args); });
}

}
