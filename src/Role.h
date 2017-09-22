#pragma once
#include"head.h"

class Role
{
private:
    static std::vector<Role*> role_vector_;
public:
	Role();
	~Role();

	short ListNum, HeadNum, IncLife, Lucky;
	char Name[10];
	short Placeholder1;
	char Nickname[10];
	short Placeholder2, Sexual, Level;

	unsigned short Exp;//18
	short CurrentHP, MaxHP, Wounded, Poison, PhyPower; //23
	short FightNum;
	short Equip[5]; // 30
	short GongTi, TeamState, Angry, IsRandomed, Moveable, SkillPoint, FollowersNumber, Appearance,
		ActionTime, Difficulty, Loyalty, Rehurt, MPType, CurrentMP, MaxMP, Attack, Speed, Defence,
		Medcine, UsePoison, MedPoison, DefPoison, Boxing, Fencing, Knife, SpecialSkill, Shader, MartialKnowledge, Morality,
		AttPoison, AttTwice, Reputation, Aptitude, ReadingBook;
	unsigned short ExpForBook; //64
	short XiangXing, Friendship, CharacterType, RTendency, MTendency, XTendency, FTendency, Faction, Master, Generation, ApprenticeOrder; //75
	short HatredFaction[2];
	short CurrentPosition, InnerPosition, InnerToward, Unuse0, ExerciseTendency, MeditationTendency, WorkTendency, OtherTendency, TemporaryPosition, TInnerPosition, TToward,
		Sx, Sy, IsGift, Contributions, Unuse5, Unuse6, Unuse7, Unuse8, Unuse9, BTNum; //98
	short LMagic[30], MagLevel[30]; //158
	NAlist TakingItem[4];
	short	ActivateMagic[10], LAction[30], IsEvent, AllEvent[9], LeaveEvent;

};

