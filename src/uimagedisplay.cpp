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
#include <map>
//#include <iostream>

#include <urbi/uobject.hh>

#include <cv.h>
#include <highgui.h>

#include <boost/noncopyable.hpp>
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>

using namespace std;
using namespace cv;
using namespace urbi;
using namespace boost;

class HighGuiEventLoopSingleton: public noncopyable {
public:
	typedef struct _WindowData {
		string name;
		enum {toRegister, toUnregister, registered, unregistered} state;
		bool operator ==(const struct _WindowData& windowData) const { return name == windowData.name; }
		bool isUnregistered() const { return state == unregistered; }
	} WindowData;
	typedef map<const UObject*, WindowData> WindowNames;
	typedef pair<const UObject*, WindowData> WindowNamesPair;
	WindowNames mUsedWindowNames;

	HighGuiEventLoopSingleton();

	void stopThreadFunction();
	void threadFunction();
	thread mThread;
	mutex mHighGUIMutex;
	mutex mRegisterWindowMutex;

	bool isUObject(const UObject*) const;
	bool isWindowName(const string&) const;

public:
	~HighGuiEventLoopSingleton();

	static HighGuiEventLoopSingleton& getInstance();
	bool registerWindow(const UObject*, const string& windowName);
	void unregisterWindow(const UObject*);

	bool showImage(const UObject*, const Mat& image);

	string getWindowName(const UObject*) const;
};

inline HighGuiEventLoopSingleton::HighGuiEventLoopSingleton() {
//	cerr << "HighGuiEventLoopSingleton::HighGuiEventLoopSingleton" << endl;
}

HighGuiEventLoopSingleton::~HighGuiEventLoopSingleton() {
//	cerr << "HighGuiEventLoopSingleton::~HighGuiEventLoopSingleton" << endl;

	destroyAllWindows();
	mUsedWindowNames.clear();
	stopThreadFunction();
}

HighGuiEventLoopSingleton& HighGuiEventLoopSingleton::getInstance() {
//	cerr << "HighGuiEventLoopSingleton::getInstance" << endl;

	static HighGuiEventLoopSingleton instance;
	return instance;
}

bool HighGuiEventLoopSingleton::registerWindow(const UObject* uobject, const string& windowName) {
//	cerr << "HighGuiEventLoopSingleton::registerWindow" << endl;

	lock_guard<mutex> lockGuard(mHighGUIMutex);

	if (mUsedWindowNames.size() == 0)
		mThread = thread(&HighGuiEventLoopSingleton::threadFunction, this);

	if(isUObject(uobject) || isWindowName(windowName) || windowName.size() == 0) {
		return false;
	}

	WindowData windowData = {windowName, WindowData::toRegister};
	mUsedWindowNames.insert(WindowNamesPair(uobject, windowData));
	mRegisterWindowMutex.unlock();
	return true;
}

void HighGuiEventLoopSingleton::unregisterWindow(const UObject* uobject) {
//	cerr << "HighGuiEventLoopSingleton::unregisterWindow" << endl;

	lock_guard<mutex> lockGuard(mHighGUIMutex);

	if (isUObject(uobject)) {
		destroyWindow(mUsedWindowNames[uobject].name);
		mUsedWindowNames.erase(uobject);
		mRegisterWindowMutex.unlock();
	}

	if (mUsedWindowNames.size() == 0)
		stopThreadFunction();
}

bool dupa(HighGuiEventLoopSingleton::WindowNamesPair d) {
	return d.second.isUnregistered();
}

void HighGuiEventLoopSingleton::threadFunction() {
	try {
		for(;;) {
			this_thread::sleep(posix_time::milliseconds(10));

			lock_guard<mutex> lockGuard(mHighGUIMutex);

			if(mRegisterWindowMutex.try_lock()) {
				// Create new windows
				for (WindowNames::iterator i = mUsedWindowNames.begin(); i != mUsedWindowNames.end(); ++i) {
					switch(i->second.state) {
					case WindowData::toRegister:
						namedWindow(i->second.name, CV_WINDOW_NORMAL | CV_WINDOW_KEEPRATIO);
						i->second.state = WindowData::registered;
						break;
					case WindowData::toUnregister:
						destroyWindow(i->second.name);
						i->second.state = WindowData::unregistered;
						break;
					case WindowData::registered:
					case WindowData::unregistered:
					default:
						break;
					}
				}
			}

			this_thread::interruption_point();

			waitKey(1);
		}
	} catch (thread_interrupted&) {
		return;
	}
}

void HighGuiEventLoopSingleton::stopThreadFunction() {
//	cerr << "HighGuiEventLoopSingleton::stopThreadFunction" << endl;

	mThread.interrupt();
	mThread.join();
}

bool HighGuiEventLoopSingleton::isUObject(const UObject* uobject) const {
//	cerr << "HighGuiEventLoopSingleton::isUObject" << endl;

	return (std::find_if(mUsedWindowNames.begin(), mUsedWindowNames.end(),
			bind(equal_to<const UObject*>(),
					bind(&WindowNames::value_type::first, _1), uobject)) != mUsedWindowNames.end());
}

bool HighGuiEventLoopSingleton::isWindowName(const string& windowName) const{
//	cerr << "HighGuiEventLoopSingleton::isWindowName" << endl;

	WindowData windowData = {windowName};
	return (std::find_if(mUsedWindowNames.begin(), mUsedWindowNames.end(),
			bind(equal_to<WindowData>(),
					bind(&WindowNames::value_type::second, _1), windowData)) != mUsedWindowNames.end());
}

bool HighGuiEventLoopSingleton::showImage(const UObject* uobject, const Mat& image) {
//	cerr << "HighGuiEventLoopSingleton::showImage" << endl;

	if (!isUObject(uobject))
		return false;

	lock_guard<mutex> lockGuard(mHighGUIMutex);
	imshow(mUsedWindowNames[uobject].name, image);

	return true;
}

string HighGuiEventLoopSingleton::getWindowName(const UObject* uobject) const {
//	cerr << "HighGuiEventLoopSingleton::getWindowName" << endl;

	if (!isUObject(uobject))
		return string();
	else
		return mUsedWindowNames.at(uobject).name;
}

class UImageDisplay: public urbi::UObject {
private:
public:
	UImageDisplay(const string& s): UObject(s) { UBindFunction(UImageDisplay, init); }
	void init(const string& windowName);

	~UImageDisplay();

	void show(UImage image);

	// Getty
	string windowName() const { return HighGuiEventLoopSingleton::getInstance().getWindowName(this); }
};

void UImageDisplay::init(const string& windowName) {

	if(!HighGuiEventLoopSingleton::getInstance().registerWindow(this, windowName))
		throw runtime_error("Unable to create window");

	UBindFunctions(UImageDisplay, show, windowName);
}

UImageDisplay::~UImageDisplay() {
	HighGuiEventLoopSingleton::getInstance().unregisterWindow(this);
}

void UImageDisplay::show(UImage image) {
	if (image.imageFormat != IMAGE_RGB)
		throw runtime_error("Unsupported image type");

	Mat bgrImage;
	cvtColor(Mat(Size(image.width, image.height), CV_8UC3, image.data), bgrImage, CV_RGB2BGR);
	HighGuiEventLoopSingleton::getInstance().showImage(this, bgrImage);
}

UStart(UImageDisplay);
