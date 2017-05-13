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

	short m_sInShip, m_sWhere, m_sMy, m_sMx, m_sSy, m_sSx, m_sMFace, m_sShipX, m_sShipY, m_sTime, m_sTimeEvent, m_sRandomEvent, m_sSFace, m_sShipFace, m_sTeamCount, m_sTeamList[config::MaxTeamMember];
	TTeamList m_RItemList[config::MAX_ITEM_AMOUNT];
	short m_sDifficulty, m_sCheating, m_sBeiyong[4];

};

