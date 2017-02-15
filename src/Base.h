#pragma once
#include <vector>
#include "Engine.h"
#include "Texture.h"

#define CONNECT(a,b) a##b

struct auto_ptr
{

};

class Base
{
public:
	Base();
	virtual ~Base();

	void LOG(const char *format, ...);

	virtual void draw() {}
	virtual void dealEvent(BP_Event& e) {}

	static std::vector<Base*> baseVector;   

	bool visible = true;

	template <class T> void safe_delete(T*& pointer)
	{
		if (pointer)
			delete pointer;
		pointer = nullptr;
	}

	virtual void init() {}

	void push(Base* b) { baseVector.push_back(b); b->init(); }
	void pop() { safe_delete(baseVector.back()); baseVector.pop_back(); }

	int x = 0, y = 0;
	void setPosition(int x, int y) { this->x = x; this->y = y; }
	int w = 0, h = 0;
	void setSize(int w, int h) { this->w = w; this->h = h; }

	bool inSide(int x, int y);

	int full = 0;

};

