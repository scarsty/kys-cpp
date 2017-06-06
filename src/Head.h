#pragma once

static class Head
{
public:
	static int inEvent;	//是否正在事件中
	static int CurEvent;
	static int CurItem;
};
struct NAlist
{
	short  Number, Amount;
};