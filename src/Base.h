#pragma once
#include <vector>
#include "Engine.h"
#include "TextureManager.h"

struct auto_ptr
{

};

class Base
{
public:
	Base();
	virtual ~Base();

	void log(const char *format, ...);

	virtual void draw() {}
	virtual void dealEvent(BP_Event& e) {}

	static std::vector<Base*> baseVector;

	bool visible = true;

	template <class T> void safe_delete(T* pointer)
	{
		if (pointer)
			delete pointer;
		pointer = nullptr;
	}

	void push(Base* b) { baseVector.push_back(b); }
	void pop() { safe_delete(baseVector.back()); baseVector.pop_back(); }

	int x = 0;
	int y = 0;
	void setPosition(int x, int y) { this->x = x; this->y = y; }

};

