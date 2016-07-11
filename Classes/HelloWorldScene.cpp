#include "HelloWorldScene.h"
#include "MainMap.h"
#include "Save.h"
#include "SubScene.h"
#include "RandomAttribute.h"
#include "battle.h"
#include <iostream>
#include <fstream>
#include "Start.h"
#include "Event.h"
#include "Error.h"

USING_NS_CC;
using namespace cocostudio;
using namespace ui;

Scene* HelloWorld::createScene()
{
    auto scene = Scene::create();
    auto layer = HelloWorld::create();
    scene->addChild(layer);
    return scene;
}
bool HelloWorld::init()
{
    if ( !Layer::init() )
    {
        return false;
    }

	auto pNode = cocostudio::GUIReader::getInstance()->widgetFromJsonFile("ui/StartUifinal/StartUifinal_1.ExportJson");
	auto button1 = (Button *)Helper::seekWidgetByName(pNode, "Button1");
	auto button2 = (Button *)Helper::seekWidgetByName(pNode, "Button2");
	auto button3 = (Button *)Helper::seekWidgetByName(pNode, "Button3");
	button1->addTouchEventListener(this, toucheventselector(HelloWorld::touchButton));
	button2->addTouchEventListener(this, toucheventselector(HelloWorld::touchButton));
	button3->addTouchEventListener(this, toucheventselector(HelloWorld::touchButton));
	this->addChild(pNode);
	///SaveGame::getInstance()->LoadR(0);
	EventManager eventManager;
	eventManager.initEventData();
	loadTexture("mmap", MyTexture2D::MainMap, 6000);
	loadTexture("smap", MyTexture2D::Scene, 6000);
	for (int i = 0; i < MyTexture2D::MaxRole; i++)
	{
		char *fightPath = new char[30];
		sprintf(fightPath, "fight/fight%03d", i);
		char *fightPathIn = new char[30];
		sprintf(fightPathIn, "fight/fight%03d/index.ka", i);
		auto file = FileUtils::getInstance();
		//std::fstream file;
		if (file->isFileExist(fightPathIn)){
			loadTexture(fightPath, MyTexture2D::Battle, 250, i);
		}
		//delete(fightPath);
		delete(fightPathIn);
	} 
	battle::getInstance()->loadSta();	
    return true;
}

void HelloWorld::touchButton(Ref* pSender, cocos2d::ui::TouchEventType eventType)
{
	auto button = dynamic_cast<Button*>(pSender);                   
	int tag = button->getTag();
	switch (eventType)
	{
	case TouchEventType::TOUCH_EVENT_ENDED:
		if (tag == 11)
		{
			auto attribute = RandomAttribute::createScene();
			auto transitionScene = TransitionCrossFade::create(0.6, attribute);
			Director::getInstance()->replaceScene(transitionScene);
			break;
		}
		else if (tag == 12)
		{
			
			if (!EventManager::getInstance()->initEventData())
			{
				Error::setError(1101,"¶ÁÈ¡ÊÂ¼þÊ§°Ü");
				Director::getInstance()->end();
				break;
			}

			auto startscen = Start::createScene();
			auto transitionScene = TransitionCrossFade::create(0.6, startscen);
			Director::getInstance()->replaceScene(transitionScene);
			break;
		}
		else if (tag == 13)
		{

			Director::getInstance()->end();
			break;
		}
		
	}
}

