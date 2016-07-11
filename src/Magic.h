#pragma once

class Magic
{
public:
	short ListNum;
	char Name[10];//6
	short Placeholder1, Useness, Book, NeedHP, MinRange, BigAmi, EventNum, SoundNum, MagicType, AmiNum, HurtType, AttAreaType,
		NeedMP, Poision, MinHurt, MaxHurt, HurtModulus, AttackModulus, MPModulus, SpeedModulus, WeaponModulus,
		IsSecret, AddMpScale, AddHpScale, MoveDistance[10], AttDistance[10];//50
	short AddHP[3], AddMP[3], AddAtt[3], AddDef[3], AddSpd[3];//65
	short MinPeg, MaxPeg, MinInjury, MaxInjury, AddMedcine, AddUsePoi, AddMedPoi, AddDefPoi, AddBoxing, AddFencing, AddKnife,
		AddSpecial, AddShader, BattleState, NeedExp[3];//82
	short MaxLevel;
	char Introduction[60];//113
	short Placeholder2, Action[5], Special[10], SpecialMod[10];//139

	Magic();
	~Magic();
};

