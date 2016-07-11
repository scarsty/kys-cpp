#pragma once

class ZhaoShi
{
public:
	struct TEffects
	{
		short x, y;
	};

	short Num, Belong, OrderNum;
	char Name[20];
	short Placeholder1, IsAtt, AttBuff, IsDef, DefBuff;
	char Introduction[46]; //40
	short Placeholder2;
	TEffects EFFects[24];

	ZhaoShi();
	~ZhaoShi();

};

