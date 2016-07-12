#pragma once
class File
{
public:
	File();
	virtual ~File();
	static void readFile(const char * filename, unsigned char** s, int* len);
};

