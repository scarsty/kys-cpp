#pragma once

#include "Common.h"

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

