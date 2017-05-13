#pragma once
#include "Base.h"
#include "Engine.h"

/**
* @file		 AppDelegate.h
* @brief	 ”¶”√¿‡
* @author    bttt

*/

class  AppDelegate : public Base
{
public:
    AppDelegate();
    virtual ~AppDelegate();
    virtual bool applicationDidFinishLaunching();
    void mainLoop(BP_Event &e);
};

