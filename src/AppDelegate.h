#pragma once
#include "Base.h"
#include "Engine.h"

class  AppDelegate : public Base
{
public:
	AppDelegate();
	virtual ~AppDelegate();
	virtual bool applicationDidFinishLaunching();
	void mainLoop(BP_Event &e);
};

