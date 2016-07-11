#include "CRotateMenu.h"
#define PI acos(-1)
float CRotateMenu::m_aniDuration = 0.3f;

CRotateMenu::CRotateMenu():m_angle(0),m_unitAngle(0),m_selectedItem(nullptr)
{
}
CRotateMenu::~CRotateMenu()
{
}

bool CRotateMenu::init()
{
	if (!Layer::init())
	{
		return false;
	}
	m_angle = 0.0f;
	this -> ignoreAnchorPointForPosition(false);
	Size s = Director::getInstance()->getWinSize();
	this -> setContentSize(s/3*2);
	this -> setAnchorPoint(Vec2(0.5f,0.5f));
	auto listener = EventListenerTouchOneByOne::create();
	listener -> onTouchBegan = CC_CALLBACK_2(CRotateMenu::onTouchBegan,this);
	listener -> onTouchMoved = CC_CALLBACK_2(CRotateMenu::onTouchMoved,this);
	listener -> onTouchEnded = CC_CALLBACK_2(CRotateMenu::onTouchEnded,this);
	getEventDispatcher()->addEventListenerWithSceneGraphPriority(listener,this);

	return true;

}

void CRotateMenu::addMenuItem(MenuItem*item)
{
	assert(item!=nullptr);
	item -> setPosition(this->getContentSize()/2);
	this -> addChild(item);
	m_items.pushBack(item);
	setUnitAngle(2*PI/m_items.size());
	reset();
	updatePositionWithAnimation();

	return;
}

void CRotateMenu::updatePosition()
{
	auto menuSize = getContentSize();
	auto disY = menuSize.height/8;
	auto disX = menuSize.width/3;
	for (int i = 0; i < m_items.size(); i++)
	{
		float x = menuSize.width/2 + disX*sin(i*m_unitAngle+getAngle());
		float y = menuSize.height/2 - disY*cos(i*m_unitAngle+getAngle());
		m_items.at(i) -> setPosition(x,y);
		m_items.at(i) -> setZOrder(-(int)y);
		//Opacity 129-255
		m_items.at(i) -> setOpacity(192+63*cos(i*m_unitAngle+getAngle()));
		m_items.at(i) -> setScale(0.75+0.25*cos(i*m_unitAngle+getAngle()));
	}
	return;
}

void CRotateMenu::updatePositionWithAnimation()
{
	//first stop all may be running actions
	for (int i = 0; i < m_items.size(); i++)
	{
		m_items.at(i) -> stopAllActions();
	}
    auto menuSize = getContentSize();  
    auto disY = menuSize.height / 8;  
    auto disX = menuSize.width / 3;  
    for (int i = 0; i < m_items.size(); i++)
	{  
        float x = menuSize.width / 2 + disX*sin(i*m_unitAngle + getAngle());  
        float y = menuSize.height / 2 - disY*cos(i*m_unitAngle + getAngle());  
        auto moveTo = MoveTo::create(m_aniDuration, Vec2(x, y));  
        m_items.at(i)->runAction(moveTo);  
        //Opacity  129~255  
        auto fadeTo = FadeTo::create(m_aniDuration, (192 + 63 * cos(i*m_unitAngle + getAngle())));  
        m_items.at(i)->runAction(fadeTo);  
        //set scale  0.5~1  
        auto scaleTo = ScaleTo::create(m_aniDuration, 0.75 + 0.25*cos(i*m_unitAngle + getAngle()));  
        m_items.at(i)->runAction(scaleTo);  
        m_items.at(i)->setZOrder(-(int)y);  
    }  
    scheduleOnce(schedule_selector(CRotateMenu::onActionEndCallback), m_aniDuration);  

    return;  
}

void CRotateMenu::reset()
{
	m_angle = 0;
}

float CRotateMenu::convertDisToAngle(float distance)
{
	float width = this -> getContentSize().width/2;
	return distance/width*getUnitAngle();
}

MenuItem* CRotateMenu::getCurrentItem()
{
	if (m_items.size()==0)
	{
		return nullptr;
	}

	//add 0.1getAngle(),prevent accuracy losed
	int index = (int)((2*PI-getAngle())/getUnitAngle()+0.1*getUnitAngle());
	index %= m_items.size();
	return m_items.at(index);
}

bool CRotateMenu::onTouchBegan(Touch*touch,Event*event)
{
	//first stop all may be running actions
	for (int i = 0; i < m_items.size(); i++)
	{
		m_items.at(i) -> stopAllActions();
	}
	if (m_selectedItem)
	{
		m_selectedItem -> unselected();
	}
	auto positon = this->convertToNodeSpace(touch->getLocation());
	auto size = this -> getContentSize();
	auto rect = Rect(0,0,size.width,size.height);
	if (rect.containsPoint(positon))
	{
		//touch item can move ,otherwise can not move
		return true;
	}
	return false;
}
void CRotateMenu::onTouchMoved(Touch*touch,Event*event)
{
	auto angle = convertDisToAngle(touch->getDelta().x);
	setAngle(getAngle()+angle);
	updatePosition();

	return;
}
void CRotateMenu::onTouchEnded(Touch*touch,Event*event)
{
	auto delta = touch ->getLocation().x - touch -> getStartLocation().x;
	rectifyPos(delta>0);
	if (convertDisToAngle(fabs(delta))<getUnitAngle()/6 && m_selectedItem)
	{
		m_selectedItem -> activate();
	}
	updatePositionWithAnimation();

	return;
}

void CRotateMenu::onActionEndCallback(float dt)
{
	m_selectedItem = getCurrentItem();
	if (m_selectedItem)
	{
		m_selectedItem -> selected();

		//add by myself 
		//m_selectedItem -> activate();
	}
}

void CRotateMenu::rectifyPos(bool forward)
{
	auto angle = getAngle();
	while (angle<0)
	{
		angle += PI*2;
	}
	while (angle>PI*2)
	{
		angle -= PI*2;
	}
	if (forward)
	{
		angle = ((int)((angle+getUnitAngle()/3*2) / getUnitAngle())) * getUnitAngle();
	}
	else
	{
		angle = ((int)((angle+getUnitAngle()/3) /getUnitAngle())) *getUnitAngle();
	}

	setAngle(angle);
}