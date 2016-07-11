#ifndef __Esc_H__
#define __Esc_H__

#include "cocos2d.h"
#include "Common.h"
#include "cocostudio/CocoStudio.h"
#include "cocos-ext.h"
#include "ui/CocosGUI.h"


USING_NS_CC;

class Esc : public CommonScene
{
private:
	bool _readed = false;
	void ReadEsc();
public:
	Esc();
	~Esc();

	enum { tag_escLayer = 10 };
	Node* pNode;
	
	virtual bool init();
	void HiddenButton();
	static Scene* createScene();
	void Esc::checkTimer2();
	void menuCloseCallback(Ref* pSender);
	void touchButton(Ref* pSender, cocos2d::ui::TouchEventType eventType);

	CREATE_FUNC(Esc);
};

#endif 