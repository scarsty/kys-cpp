#ifndef _COLLECTOR_H_
#define _COLLECTOR_H_
#include "cocos2d.h"
#include <list>
USING_NS_CC;
class Collector : public Node{
private:
	Collector();
public:
	static Collector* create();
private:
	bool init();
	void update(float delta);
public:
	void addSource(Node* node);
	void setCallback(std::function<void()> callback);
	void setFloatTime(float time);
private:
	std::list<Node*> m_sources;
	std::function<void()> m_callback;
	float m_time;
};
#endif