#pragma once
#include "cocos2d.h"
#include <vector>
USING_NS_CC;
using namespace std;

struct ErrorNote
{
	int code;
	string note;
};

static class Error
{
public:
	Error();
	static void setError(int code,string note);

private:
	static int _count;
	static vector<ErrorNote> _notes;
};
