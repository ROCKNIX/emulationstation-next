#pragma once
#ifndef ES_APP_CLOUD_OFFER_H
#define ES_APP_CLOUD_OFFER_H

// A question a cloud script asked us to put to the player once its run is
// over (">>> offer <name>|<arg>|..."), and the one place the words and the
// buttons for it live.
//
// Two surfaces run the same scripts: the card (ThreadedCloudSync), for the
// quick save actions in GAME SETTINGS, and the page (GuiCloudTransfer), for
// the long transfers under MANAGE CLOUD STORAGE. Both reach cloud_restore,
// so both can be handed the same question -- and only the card ever asked
// it, because each surface read the protocol with a parser of its own
// (#145). The dialog is here so the two cannot say it differently.

#include <string>
#include <vector>

class Window;

namespace CloudOffer
{
	// Put `offer` to the player, with whatever the line carried after its
	// name. Safe from either thread: the dialog is pushed on the interface
	// thread whoever calls.
	//
	// An offer name this build does not know raises nothing. A script newer
	// than the image is the normal way that happens, and a dialog nobody
	// can answer is worse than a question that was not asked.
	void present(Window* window, const std::string& offer, const std::vector<std::string>& args);
}

#endif // ES_APP_CLOUD_OFFER_H
