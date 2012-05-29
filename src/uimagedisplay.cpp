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
private:
	typedef map<const UObject*, string> WindowNames;
	typedef pair<const UObject*, string> WindowNamesPair;
	WindowNames mUsedWindowNames;

	HighGuiEventLoopSingleton();

	void stopThreadFunction();
	void threadFunction();
	thread mThread;
	mutex mMutex;

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

	lock_guard<mutex> lockGuard(mMutex);

	if (mUsedWindowNames.size() == 0)
		mThread = thread(&HighGuiEventLoopSingleton::threadFunction, this);

	if(isUObject(uobject) || isWindowName(windowName) || windowName.size() == 0) {
		return false;
	}

	namedWindow(windowName, CV_WINDOW_NORMAL | CV_WINDOW_KEEPRATIO);
	mUsedWindowNames.insert(WindowNamesPair(uobject, windowName));
	return true;
}

void HighGuiEventLoopSingleton::unregisterWindow(const UObject* uobject) {
//	cerr << "HighGuiEventLoopSingleton::unregisterWindow" << endl;

	lock_guard<mutex> lockGuard(mMutex);

	if (isUObject(uobject)) {
		destroyWindow(mUsedWindowNames[uobject]);
		mUsedWindowNames.erase(uobject);
	}

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

	return (std::find_if(mUsedWindowNames.begin(), mUsedWindowNames.end(),
			bind(equal_to<string>(),
					bind(&WindowNames::value_type::second, _1), windowName)) != mUsedWindowNames.end());
}

bool HighGuiEventLoopSingleton::showImage(const UObject* uobject, const Mat& image) {
//	cerr << "HighGuiEventLoopSingleton::showImage" << endl;

	if (!isUObject(uobject))
		return false;

	lock_guard<mutex> lockGuard(mMutex);
	imshow(mUsedWindowNames[uobject], image);

	return true;
}

string HighGuiEventLoopSingleton::getWindowName(const UObject* uobject) const {
//	cerr << "HighGuiEventLoopSingleton::getWindowName" << endl;

	if (!isUObject(uobject))
		return string();
	else
		return mUsedWindowNames.at(uobject);
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
