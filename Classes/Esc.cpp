#include "Esc.h"
#include "Save.h"

Esc::Esc()
{
	if (!_readed)
	{
		ReadEsc();
	}
	_readed = true;
}

Esc::~Esc()
{
}

void Esc::ReadEsc()
{
	log("Reading Esc backgroud.");	
}

bool Esc::init()
{
	if (!Layer::init())
	{
		return false;
	}
	Size visibleSize = Director::getInstance()->getVisibleSize();
	Point origin = Director::getInstance()->getVisibleOrigin();
	FileUtils::getInstance()->addSearchPath("ui/system1");
	pNode = CSLoader::createNode("ui/system1/MainScene.csb"); 
	this->addChild(pNode);
	
	HiddenButton();
	auto button1 = (Button *)Helper::seekWidgetByTag((Widget*)pNode, 6);
	button1->addTouchEventListener(this, toucheventselector(Esc::touchButton));
	auto button2 = (Button *)Helper::seekWidgetByTag((Widget*)pNode, 7);
	button2->addTouchEventListener(this, toucheventselector(Esc::touchButton));
	auto button3 = (Button *)Helper::seekWidgetByTag((Widget*)pNode, 8);
	button3->addTouchEventListener(this, toucheventselector(Esc::touchButton));
	auto button4= (Button *)Helper::seekWidgetByTag((Widget*)pNode, 9);
	button4->addTouchEventListener(this, toucheventselector(Esc::touchButton));
	auto button5 = (Button *)Helper::seekWidgetByTag((Widget*)pNode, 10);
	button5->addTouchEventListener(this, toucheventselector(Esc::touchButton));

	keypressing = EventKeyboard::KeyCode::KEY_NONE;
	auto keyboardListener = EventListenerKeyboard::create();
	keyboardListener->onKeyPressed = CC_CALLBACK_2(CommonScene::keyPressed, this);
	keyboardListener->onKeyReleased = CC_CALLBACK_2(CommonScene::keyReleased, this);
	_eventDispatcher->addEventListenerWithSceneGraphPriority(keyboardListener, this);

	return true;
}

void Esc::touchButton(Ref* pSender, cocos2d::ui::TouchEventType eventType)
{
	auto button = dynamic_cast<Button*>(pSender);
	int tag = button->getTag();
	switch (eventType)
	{
	case TouchEventType::TOUCH_EVENT_ENDED:
		if (tag == 6)
		{
			auto button1 = (Button *)Helper::seekWidgetByTag((Widget*)pNode, 21);
			button1->setVisible(true);
			auto button2 = (Button *)Helper::seekWidgetByTag((Widget*)pNode, 22);
			button2->setVisible(true);
			auto button3 = (Button *)Helper::seekWidgetByTag((Widget*)pNode, 23);
			button3->setVisible(true);
			auto button4 = (Button *)Helper::seekWidgetByTag((Widget*)pNode, 24);
			button4->setVisible(true);
			auto button5= (Button *)Helper::seekWidgetByTag((Widget*)pNode, 25);
			button5->setVisible(true);
			auto button6 = (Button *)Helper::seekWidgetByTag((Widget*)pNode, 26 );
			button6->setVisible(true);
		}
		if (tag == 7)
		{
			auto button7 = (Button *)Helper::seekWidgetByTag((Widget*)pNode, 16);
			button7->setVisible(true);
			auto button8= (Button *)Helper::seekWidgetByTag((Widget*)pNode, 17);
			button8->setVisible(true);
			auto button9 = (Button *)Helper::seekWidgetByTag((Widget*)pNode, 18);
			button9->setVisible(true);
			auto button10 = (Button *)Helper::seekWidgetByTag((Widget*)pNode, 19);
			button10->setVisible(true);
			auto button11 = (Button *)Helper::seekWidgetByTag((Widget*)pNode, 20);
			button11->setVisible(true);
		}
		if (tag == 8)
		{
			auto button12 = (Button *)Helper::seekWidgetByTag((Widget*)pNode, 27);
			button12->setVisible(true);
		}
		if (tag ==9)
		{
			auto button13 = (Button *)Helper::seekWidgetByTag((Widget*)pNode, 28);
			button13->setVisible(true);
			auto button14 = (Button *)Helper::seekWidgetByTag((Widget*)pNode, 29);
			button14->setVisible(true);
			auto button15= (Button *)Helper::seekWidgetByTag((Widget*)pNode, 30);
			button15->setVisible(true);
		}
		if (tag == 10)
		{
			auto button16 = (Button *)Helper::seekWidgetByTag((Widget*)pNode, 31);
			button16->setVisible(true);
			auto button17 = (Button *)Helper::seekWidgetByTag((Widget*)pNode, 32);
			button17->setVisible(true);
		}
	}
}

void Esc::HiddenButton()
{
	auto button1 = (Button *)Helper::seekWidgetByTag((Widget*)pNode, 21);
	button1->setVisible(false);
	auto button2 = (Button *)Helper::seekWidgetByTag((Widget*)pNode, 22);
	button2->setVisible(false);
	auto button3 = (Button *)Helper::seekWidgetByTag((Widget*)pNode, 23);
	button3->setVisible(false);
	auto button4 = (Button *)Helper::seekWidgetByTag((Widget*)pNode, 24);
	button4->setVisible(false);
	auto button5 = (Button *)Helper::seekWidgetByTag((Widget*)pNode, 25);
	button5->setVisible(false);
	auto button6 = (Button *)Helper::seekWidgetByTag((Widget*)pNode, 26);
	button6->setVisible(false);
	auto button7 = (Button *)Helper::seekWidgetByTag((Widget*)pNode, 16);
	button7->setVisible(false);
	auto button8 = (Button *)Helper::seekWidgetByTag((Widget*)pNode, 17);
	button8->setVisible(false);
	auto button9 = (Button *)Helper::seekWidgetByTag((Widget*)pNode, 18);
	button9->setVisible(false);
	auto button10 = (Button *)Helper::seekWidgetByTag((Widget*)pNode, 19);
	button10->setVisible(false);
	auto button11 = (Button *)Helper::seekWidgetByTag((Widget*)pNode, 20);
	button11->setVisible(false);
	auto button12 = (Button *)Helper::seekWidgetByTag((Widget*)pNode, 27);
	button12->setVisible(false);
	auto button13 = (Button *)Helper::seekWidgetByTag((Widget*)pNode, 28);
	button13->setVisible(false);
	auto button14 = (Button *)Helper::seekWidgetByTag((Widget*)pNode, 29);
	button14->setVisible(false);
	auto button15 = (Button *)Helper::seekWidgetByTag((Widget*)pNode, 30);
	button15->setVisible(false);
	auto button16 = (Button *)Helper::seekWidgetByTag((Widget*)pNode, 31);
	button16->setVisible(false);
	auto button17 = (Button *)Helper::seekWidgetByTag((Widget*)pNode, 32);
	button17->setVisible(false);
}



void Esc::menuCloseCallback(Ref* pSender)
{
#if (CC_TARGET_PLATFORM == CC_PLATFORM_WP8) || (CC_TARGET_PLATFORM == CC_PLATFORM_WINRT)
	MessageBox("You pressed the close button. Windows Store Apps do not implement a close button.", "Alert");
	return;
#endif
	auto p = dynamic_cast<MenuItemImage*> (pSender);

	if (p == nullptr)
		return;

	switch (p->getTag())
	{
	case 1:
	{
			 Save::getInstance()->SaveR(0);
			  break;
	}
	}
#if (CC_TARGET_PLATFORM == CC_PLATFORM_IOS)
	exit(0);
#endif
}
Scene* Esc::createScene()
{
	auto scene = Scene::create();
	auto layer = Esc::create();
	scene->addChild(layer, 0);
	return scene;
}

void Esc::checkTimer2()
{
	switch (keypressing)
	{
	case EventKeyboard::KeyCode::KEY_LEFT_ARROW:
	{
		break;
	}
	case EventKeyboard::KeyCode::KEY_RIGHT_ARROW:
	{
		break;
	}
	case EventKeyboard::KeyCode::KEY_UP_ARROW:
	{
		break;
	}
	case EventKeyboard::KeyCode::KEY_DOWN_ARROW:
	{
	   break;
	}
	case EventKeyboard::KeyCode::KEY_ESCAPE:
	{
		log("escape");
		Director::getInstance()->popScene();
		break;
	}
	case EventKeyboard::KeyCode::KEY_SPACE:
	case EventKeyboard::KeyCode::KEY_KP_ENTER:
	{
		 break;
	}
	default:
	{
	}
	}
}