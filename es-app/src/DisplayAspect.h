#pragma once
#ifndef ES_APP_DISPLAY_ASPECT_H
#define ES_APP_DISPLAY_ASPECT_H

#include <string>

class FileData;
class GuiComponent;
class SystemData;

// How a picture RetroArch wrote at the core's native size is shown (fork
// #243, D-UI-080; #245, D-UI-081): at the width-to-height of the system
// the game runs on (0 means the file's own), turned by the quarter turns
// the display turned the game's frame. A save-state thumbnail takes its
// game's; a screenshot under the SCREENSHOTS entry takes the game it was
// taken in, found by the content name RetroArch put in the file name.
namespace DisplayAspect
{
	struct Transform
	{
		float aspect = 0.0f;
		int turns = 0;
	};

	float forSystem(SystemData* system);
	Transform forGame(FileData* game);
	Transform forScreenshot(FileData* screenshot);
	Transform forScreenshotPath(const std::string& path);
	// Empty the screenshot cache, so a game whose rotation record has just
	// been written (CaptureRotation::recordAfterSession) turns its
	// screenshots on the next look rather than after a restart (#258 PL-019).
	void forgetScreenshots();

	// Give every ImageComponent under root that shows imagePath the
	// transform, and every other one its file's own; a component bound to
	// another picture is untouched. Theme extras bound with {game:image}
	// live in the list view, the md_image in the details container --
	// both call this.
	void applyToBoundImages(GuiComponent* root, const std::string& imagePath, const Transform& transform);
}

#endif // ES_APP_DISPLAY_ASPECT_H
