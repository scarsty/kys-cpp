#pragma once

#include "config.h"

class Character
{
public:

	Character();
	~Character();
	//注意请将要存储在存档的数据放在前面

	short ListNum = 999, HeadNum, IncLife, Lucky;
	char Name[10];
	short Placeholder1;
	char Nickname[10];
	short Placeholder2, Sexual, Level;

	unsigned short Exp;//18
	short CurrentHP, MaxHP, Wounded, Poison, PhyPower; //23
	short FightNum;
	short Equip[config::MaxEquipNum]; // 30
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
	short TakingItem[4], TakingItemAmount[4], ActivateMagic[10], LAction[30];
	short IsEvent;
	short AllEvent[9];
	short LeaveEvent;
	short LaoLian, QiangZhuang, NeiJia, QiangGong, JianGU, QingLing, QuanShi, JianKe, Daoke, YiBing, AnJian, YIShi, DuRen, HuiXue, HuiNei,
		HuiTI, BaoZao, PeiHe, WuXue, TuPo, LengJing, BaiBian, PoQi, ZhaoMen, BianHuan, FanGong, QiGong, YingGong, LingHuo, XingQi, ShenFa,
		FenFa, ZhanYi, JingZhun, JiSu, KuangBao, ShouFa, LianHuan, WaJie, GuShou; // 257
	short TianMing, XingXiu;
	short ZhuanChang[5]; //264
	short TeXing[10]; //274


	static void getInstance();
	void loadData();
	
};

