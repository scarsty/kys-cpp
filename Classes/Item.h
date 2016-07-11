#pragma once

class Item
{

public:
	short ListNum;
	char Name[20];
	short Placeholder1, ExpOfMagic, SetNum, BattleEffect, WineEffect, NeedSexual, Rehurt, Unuse[4]; //21
	char Introduction[30]; //36
	short Placeholder2, Magic, AmiNum, User, EquipType, ShowIntro, ItemType, Inventory, Price, EventNum; //46
	short AddCurrentHP, AddMaxHP, AddPoi, AddPhyPower, ChangeMPType, AddCurrentMP, AddMaxMP, AddAttack, AddSpeed,
		AddDefence, AddMedcine, AddUsePoi, AddMedPoison, AddDefPoi; //60
	short AddBoxing, AddFencing, AddKnife, AddSpecial, AddShader, AddMKnowledge, AddMorality, AddAttTwice, AddAttPoi; //69
	short OnlyPracRole, NeedMPType, NeedMP, NeedAttack, NeedSpeed, NeedUsePoi, NeedMedcine, NeedMedPoi; //77
	short NeedBoxing, NeedFencing, NeedKnife, NeedSpecial, NeedShader, NeedAptitude; //83
	short NeedExp, Count, Rate; //86
	short NeedItem[5], NeedItemAmount[5];

	Item();
	~Item();

};

