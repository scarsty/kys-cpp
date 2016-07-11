/*
参考 http://blog.csdn.net/ccy0815ccy/article/details/41519767

设计说明

1.菜单项(MenuItem)平均分布在椭圆（类似)上

2.椭圆长轴为2/3width,短轴为2/8 height

3.最前面的菜单项Scale=1,opacity=255,最后面Scale=0.5,opacity=129.其它位置根据三角函数变换(updatePosition中实现)

4.默认最前面菜单被选中(selected)

5.单位角度(unitAngle)是2*PI/菜单项的数量

6.滑动一个width，菜单旋转两个单位角度

7.Touch结束会自动调整位置，保证最前面位置有菜单项

8.滑动超过1/3单位角度会向前舍入

9.移动小于1/6单位角度会判定点击菜单

10.默认菜单大小不是全屏，而是屏幕的2/3,通过Node::setContentSize()设置

使用

使用这个菜单只要知道两个函数

1.构造函数

RotateMenu::create()(由CREATE_FUNC创建)

2.添加MenuItem

void addMenuItem(cocos2d::MenuItem *item);
*/


#ifndef __CROTATE_MENU__
#define __CROTATE_MENU__

#include "cocos2d.h"
USING_NS_CC;

class CRotateMenu:public Layer
{
public:
	CREATE_FUNC(CRotateMenu);
	bool init();

	//add menuitems
	void addMenuItem(MenuItem *item);

	//update position
	void updatePositionWithAnimation();
	void updatePosition();

	//rectify position ,true move forward,false move backward,
	//only when move distance>1/3 effect
	void rectifyPos(bool forward);

	//reset ,set rotate angle = 0;
	void reset();

	CRotateMenu();
	~CRotateMenu();
private:
	CC_SYNTHESIZE(float,m_angle,Angle);

	//unit angle
	CC_SYNTHESIZE(float,m_unitAngle,UnitAngle);

	//convert moved diatance to angle
	float convertDisToAngle(float distance);

	//get current effective(selected) menuItem
	MenuItem*getCurrentItem();
private:
	Vector<MenuItem*> m_items;
	virtual bool onTouchBegan(Touch*touch,Event*event);
	virtual void onTouchMoved(Touch*touch,Event*event);
	virtual void onTouchEnded(Touch*touch,Event*event);
	void onActionEndCallback(float dt);
	MenuItem *m_selectedItem;
	static float m_aniDuration ;
};


#endif