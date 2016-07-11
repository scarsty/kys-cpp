#pragma once

class Faction
{
	
public:
	short Num;
	char Name[20];
	short Placeholder1, BaseAmount, MainBase, Head, MemberAmount, Reputation, JusticeValue; //17
	short Resource[10], AddResource[10], FactionNeiGong[20]; //57
	short Friendship[40]; //97
	short Job[10]; //107
	short ExpansionTendency, MemberTendency, MemberGrow, Flag, MeleBeginPic, MelePic, FemeleBeginPic, FemelePic, Sexy, Identity, EndEvent, Alliance, Unuse1, Unuse2,
		Unuse3, Unuse4;

	Faction();
	~Faction();

};

