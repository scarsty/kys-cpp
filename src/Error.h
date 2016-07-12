#pragma once

#include <vector>

struct ErrorNote
{
	int code;
	std::string note;
};

static class Error
{
public:
	Error();
	static void setError(int code,std::string note);

private:
	static int _count;
	static std::vector<ErrorNote> _notes;
};
