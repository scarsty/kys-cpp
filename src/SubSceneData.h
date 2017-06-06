#pragma once
#include "config.h"
#include"head.h"


class SceneData
{
public:
	SceneData();
	~SceneData();
    short ListNum;
    char Name[10];
    short Placeholder1, ExitMusic, EntranceMusic, Pallet, EnterCondition; //10
    short MainEntranceY1, MainEntranceX1, MainEntranceY2, MainEntranceX2, EntranceY, EntranceX, ExitY[3], ExitX[3]; //22
    short MapMode, Unuse, Faction, IsBattle, MaxPraFieldAmount, PraFieldAmount, MaxStudyAmount, StudyAmount; //30
    short PraFieldX[5], PraFieldY[5], StudyX[5], StudyY[5]; //50
    short IsMeditation, MeditationX, MeditationY, IsAlchemy, AlchemyX, AlchemyY, IsFactory;
    short FactoryX, FactoryY, FlagX, FlagY, AlchemyPro, MeditationPro, DefBuff, DefBuild; //65
    short AddResourse[10], Connection[10];
};


