#pragma once

#include <vector>

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
