// UImageDisplay is a module for Urbi to display images using
// OpenCV highgui module
// Copyright (C) 2012 Adam Oleksy

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

// Usage:
// var u = UImageDisplay.new("Cam 0");
// u.show(uimage);

#include <algorithm>
#include <string>
#include <list>

#include <urbi/uobject.hh>

#include <cv.h>
#include <highgui.h>

#include <boost/noncopyable.hpp>
#include <boost/thread.hpp>

using namespace std;
using namespace cv;
using namespace urbi;
using namespace boost;

class HighGuiEventLoopSingleton: public noncopyable {
private:
	list<string> mUsedWindowNames;

	HighGuiEventLoopSingleton() {}

	void stopThreadFunction();
	void threadFunction();
	thread mThread;
	mutex mMutex;

public:
	~HighGuiEventLoopSingleton() { stopThreadFunction(); }

	static HighGuiEventLoopSingleton& getInstance();
	bool registerWindow(const string& windowName);
	void unregisterWindow(const string& windowName);
};

HighGuiEventLoopSingleton& HighGuiEventLoopSingleton::getInstance() {
	static HighGuiEventLoopSingleton instance;
	return instance;
}

bool HighGuiEventLoopSingleton::registerWindow(const string& windowName) {
	lock_guard<mutex> lockGuard(mMutex);

	if (mUsedWindowNames.size() == 0)
		mThread = thread(&HighGuiEventLoopSingleton::threadFunction, this);

	if(std::find(mUsedWindowNames.begin(), mUsedWindowNames.end(), windowName) != mUsedWindowNames.end()) {
		return false;
	}

	namedWindow(windowName, CV_WINDOW_NORMAL | CV_WINDOW_KEEPRATIO);
	mUsedWindowNames.push_back(windowName);
	return true;
}

void HighGuiEventLoopSingleton::unregisterWindow(const string& windowName) {
	lock_guard<mutex> lockGuard(mMutex);

	mUsedWindowNames.remove(windowName);
	destroyWindow(windowName);

	if (mUsedWindowNames.size() == 0)
		stopThreadFunction();
}

void HighGuiEventLoopSingleton::threadFunction() {
	try {
		for(;;) {
			this_thread::sleep(posix_time::milliseconds(10));
			lock_guard<mutex> lockGuard(mMutex);
			this_thread::interruption_point();
			waitKey(1);
		}
	} catch (thread_interrupted&) {
		return;
	}
}

void HighGuiEventLoopSingleton::stopThreadFunction() {
	mThread.interrupt();
	mThread.join();
}

class UImageDisplay: public urbi::UObject {
private:
	string mWindowName;
public:
	UImageDisplay(const string& s): UObject(s) { UBindFunction(UImageDisplay, init); }
	void init(const string& windowName);

	~UImageDisplay();

	UVar image;
	void show(UVar& src);

	// Getty
	string windowName() const { return mWindowName; };
};

void UImageDisplay::init(const string& windowName) {
	mWindowName = windowName;

	if(!HighGuiEventLoopSingleton::getInstance().registerWindow(mWindowName))
		throw runtime_error("Unable to create window");

	UBindFunctions(UImageDisplay, show, windowName);
	UBindVar(UImageDisplay, image);
	UNotifyChange(image, &UImageDisplay::show);
}

UImageDisplay::~UImageDisplay() {
	HighGuiEventLoopSingleton::getInstance().unregisterWindow(mWindowName);
}

void UImageDisplay::show(UVar& src) {
	Mat i(640, 480, CV_8UC3);
	imshow(mWindowName, i);
	waitKey(10);
}

UStart(UImageDisplay);
