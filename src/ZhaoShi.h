#pragma once
#include"head.h"
class ZhaoShi
{
public:
	

	short Num, Belong, OrderNum;
	char Name[20];
	short Placeholder1, IsAtt, AttBuff, IsDef, DefBuff;
	char Introduction[46]; //40
	short Placeholder2;
	NAlist EFFects[24];

	ZhaoShi();
	~ZhaoShi();

};

