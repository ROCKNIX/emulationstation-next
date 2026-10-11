//
// Created by matthieu on 03/08/15.
//

#ifndef EMULATIONSTATION_ALL_GUILOADING_H
#define EMULATIONSTATION_ALL_GUILOADING_H

#include "Window.h"
#include "GuiComponent.h"
#include "components/MenuComponent.h"
#include "components/BusyComponent.h"
#include "guis/GuiMsgBox.h"
#include <string>
#include <thread>
#include <mutex>
#include "Settings.h"
#include "animations/LambdaAnimation.h"

class IGuiLoadingHandler
{
public:
	virtual void setText(const std::string& text) = 0;
};

template<typename T>
class GuiLoading : public GuiComponent, public IGuiLoadingHandler
{
public:
	GuiLoading(Window *window, const std::string title, const std::function<T(IGuiLoadingHandler*)> &func, const std::function<void(T)> &func2 = nullptr, const std::function<void()> &onCancel = nullptr)
		: GuiComponent(window), mBusyAnim(window), mFunc(func), mFunc2(func2), mOnCancel(onCancel)
	{
		setSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
		setTag("GuiLoading");
	
		mText = title;
		mTextChanged = false;
		mCancelRequested = false;

		mRunning = true;
		mHandle = new std::thread(&GuiLoading::threadLoading, this);
		mBusyAnim.setText(title);
		mBusyAnim.setSize(mSize);

		mBusyAnim.setOpacity(0);
		auto fadeFunc = [this](float t) { mBusyAnim.setOpacity((unsigned char) (Math::easeOutCubic(t) * 255.0f)); };
		setAnimation(new LambdaAnimation(fadeFunc, 250), 0, [this] { mBusyAnim.setOpacity(255); });
	}
	
	~GuiLoading()
	{
		mRunning = false;
		mHandle->join();
	}

	void setText(const std::string& text) override
	{		
		mTextMutex.lock();

		if (mText != text)
		{
			mText = text;
			mTextChanged = true;
		}

		mTextMutex.unlock();
	}

	void render(const Transform4x4f &parentTrans) override
	{
		if (!mRunning || !mVisible)
			return;
	
		Transform4x4f trans = parentTrans * getTransform();
		renderChildren(trans);
	
		Renderer::setMatrix(trans);
		mBusyAnim.render(trans);
	}

	bool input(InputConfig *config, Input input) override
	{
		// Only when the caller passed a handler. It should make mFunc return;
		// deleting this GUI here would block on the worker thread.
		if (mOnCancel != nullptr && input.value != 0 && config->isMappedTo(BUTTON_BACK, input))
		{
			if (!mCancelRequested)
			{
				mCancelRequested = true;
				mOnCancel();
			}

			return true;
		}

		return false;
	}
	
	void update(int deltaTime) override
	{
		GuiComponent::update(deltaTime);

		updateText();

		mBusyAnim.update(deltaTime);

		if (!mRunning)
		{
			if (mFunc2)
			{
				Window* window = mWindow;
				auto func = mFunc2;
				auto ret = result;

				delete this;

				func(ret);
			}
			else
				delete this;
		}
	}
	
private:
	void threadLoading()
	{
		result = mFunc(this);
		mRunning = false;
	}

	void updateText()
	{
		mTextMutex.lock();

		if (mTextChanged)
		{
			mBusyAnim.setText(mText);
			mBusyAnim.setSize(mSize);
			mTextChanged = false;
		}

		mTextMutex.unlock();
	}

    BusyComponent	mBusyAnim;
    std::thread*	mHandle;
    bool			mRunning;

	std::mutex		mTextMutex;
	std::string		mText;
	bool			mTextChanged;

    const std::function<T(IGuiLoadingHandler*)> mFunc;
    const std::function<void(T)> mFunc2;
    const std::function<void()> mOnCancel;
    bool			mCancelRequested;
    T result;
};

#endif //EMULATIONSTATION_ALL_GUILOADING_H
