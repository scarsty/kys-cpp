#pragma once
#include <vector>
#include "Engine.h"
#include "Texture.h"

#define CONNECT(a,b) a##b

struct auto_ptr
{

};

class Base		//游戏绘制基础类，凡需要显示画面的，均继承自此
{
public:
    static std::vector<Base*> base_vector_;   //所有需要绘制的内容都存储在m_vcBase中
    bool visible_ = true;
public:
	Base();
	virtual ~Base();

	void LOG(const char *format, ...);

	virtual void draw() {}
	virtual void dealEvent(BP_Event& e) {}



	template <class T> void safe_delete(T*& pointer)
	{
		if (pointer)
			delete pointer;
		pointer = nullptr;
	}

	virtual void init() {}

	void push(Base* b) { base_vector_.push_back(b); b->init(); }
	void pop() { safe_delete(base_vector_.back()); base_vector_.pop_back(); }
	static Base* getCurentBase() { return base_vector_.at(0); }

	//int m_nx = 0, m_ny = 0;
	//void setPosition(int x, int y) { this->m_nx = x; this->m_ny = y; }
	//int m_nw = 0, m_nh = 0;
	//void setSize(int w, int h) { this->m_nw = w; this->m_nh = h; }

    BP_Rect rect_;
    void setRect(int x, int y, int w, int h) { rect_ = BP_Rect{ x,y,w,h }; }
	bool inSide(int x, int y);

	int full_window_ = 0;  //不为0时表示当前画面为起始层，低于本画面的层将不予显示

};

