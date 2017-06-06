#pragma once

#include "config.h"
#include"head.h"
class BasicData
{

public:
	BasicData();
	~BasicData();

	short m_sInShip, m_sWhere, m_sMy, m_sMx, m_sSy, m_sSx, m_sMFace, m_sShipX, m_sShipY, m_sTime, m_sTimeEvent, m_sRandomEvent, m_sSFace, m_sShipFace, m_sTeamCount, m_sTeamList[config::MaxTeamMember];
	NAlist m_RItemList[config::MAX_ITEM_AMOUNT];
	short m_sDifficulty, m_sCheating, m_sBeiyong[4];

};

