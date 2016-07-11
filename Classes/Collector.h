#pragma once


#include <list>

class Collector : public Node
{
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
