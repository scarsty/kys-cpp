#pragma once

#include "config.h"

class BasicData
{
public:
	BasicData();
	~BasicData();

	struct TTeamList
	{
		short  Number, Amount;
	};

	short InShip, Where, My, Mx, Sy, Sx, MFace, ShipX, ShipY, Time, TimeEvent, RandomEvent, SFace, ShipFace, TeamCount, TeamList[Config::MaxTeamMember];
	TTeamList RItemList[Config::MAX_ITEM_AMOUNT];
	short Difficulty, Cheating, Beiyong[4];

};

