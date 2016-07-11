#ifndef __HELLOWORLD_SCENE_H__
#define __HELLOWORLD_SCENE_H__

#include "cocos2d.h"
#include "Common.h"
#include "cocostudio/CocoStudio.h"
#include "cocos-ext.h"
#include "ui/CocosGUI.h"

USING_NS_CC;

class HelloWorld : public CommonScene
{
public:
    static cocos2d::Scene* createScene();
    virtual bool init();  
	void touchButton(Ref* pSender, cocos2d::ui::TouchEventType eventType);
    CREATE_FUNC(HelloWorld);
};

#endif
