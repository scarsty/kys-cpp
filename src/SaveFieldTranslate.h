#pragma once
#include "Save.h"
#include <string>

class RoleTranslate {
public:
	static std::string getVal(Role* r, const std::string& chn)
	{
		if (chn == "編號")
		{
			return std::to_string(r->ID);
		}
		else if (chn == "頭像")
		{
			return std::to_string(r->HeadID);
		}
		else if (chn == "生命成長")
		{
			return std::to_string(r->IncLife);
		}
		else if (chn == "無用")
		{
			return std::to_string(r->UnUse);
		}
		else if (chn == "名字")
		{
			return r->Name;
		}
		else if (chn == "外號")
		{
			return r->Nick;
		}
		else if (chn == "性別")
		{
			return std::to_string(r->Sexual);
		}
		else if (chn == "等級")
		{
			return std::to_string(r->Level);
		}
		else if (chn == "經驗")
		{
			return std::to_string(r->Exp);
		}
		else if (chn == "生命")
		{
			return std::to_string(r->HP);
		}
		else if (chn == "生命最大值")
		{
			return std::to_string(r->MaxHP);
		}
		else if (chn == "內傷")
		{
			return std::to_string(r->Hurt);
		}
		else if (chn == "中毒")
		{
			return std::to_string(r->Poison);
		}
		else if (chn == "體力")
		{
			return std::to_string(r->PhysicalPower);
		}
		else if (chn == "物品修煉點數")
		{
			return std::to_string(r->ExpForMakeItem);
		}
		else if (chn == "武器")
		{
			return std::to_string(r->Equip0);
		}
		else if (chn == "防具")
		{
			return std::to_string(r->Equip1);
		}
		else if (chn == "動作幀數1")
		{
			return std::to_string(r->Frame[0]);
		}
		else if (chn == "動作幀數2")
		{
			return std::to_string(r->Frame[1]);
		}
		else if (chn == "動作幀數3")
		{
			return std::to_string(r->Frame[2]);
		}
		else if (chn == "動作幀數4")
		{
			return std::to_string(r->Frame[3]);
		}
		else if (chn == "動作幀數5")
		{
			return std::to_string(r->Frame[4]);
		}
		else if (chn == "動作幀數6")
		{
			return std::to_string(r->Frame[5]);
		}
		else if (chn == "動作幀數7")
		{
			return std::to_string(r->Frame[6]);
		}
		else if (chn == "動作幀數8")
		{
			return std::to_string(r->Frame[7]);
		}
		else if (chn == "動作幀數9")
		{
			return std::to_string(r->Frame[8]);
		}
		else if (chn == "動作幀數10")
		{
			return std::to_string(r->Frame[9]);
		}
		else if (chn == "動作幀數11")
		{
			return std::to_string(r->Frame[10]);
		}
		else if (chn == "動作幀數12")
		{
			return std::to_string(r->Frame[11]);
		}
		else if (chn == "動作幀數13")
		{
			return std::to_string(r->Frame[12]);
		}
		else if (chn == "動作幀數14")
		{
			return std::to_string(r->Frame[13]);
		}
		else if (chn == "動作幀數15")
		{
			return std::to_string(r->Frame[14]);
		}
		else if (chn == "內力性質")
		{
			return std::to_string(r->MPType);
		}
		else if (chn == "內力")
		{
			return std::to_string(r->MP);
		}
		else if (chn == "內力最大值")
		{
			return std::to_string(r->MaxMP);
		}
		else if (chn == "攻擊力")
		{
			return std::to_string(r->Attack);
		}
		else if (chn == "輕功")
		{
			return std::to_string(r->Speed);
		}
		else if (chn == "防禦力")
		{
			return std::to_string(r->Defence);
		}
		else if (chn == "醫療")
		{
			return std::to_string(r->Medicine);
		}
		else if (chn == "用毒")
		{
			return std::to_string(r->UsePoison);
		}
		else if (chn == "解毒")
		{
			return std::to_string(r->Detoxification);
		}
		else if (chn == "抗毒")
		{
			return std::to_string(r->AntiPoison);
		}
		else if (chn == "拳掌")
		{
			return std::to_string(r->Fist);
		}
		else if (chn == "御劍")
		{
			return std::to_string(r->Sword);
		}
		else if (chn == "耍刀")
		{
			return std::to_string(r->Knife);
		}
		else if (chn == "特殊")
		{
			return std::to_string(r->Unusual);
		}
		else if (chn == "暗器")
		{
			return std::to_string(r->HiddenWeapon);
		}
		else if (chn == "武學常識")
		{
			return std::to_string(r->Knowledge);
		}
		else if (chn == "品德")
		{
			return std::to_string(r->Morality);
		}
		else if (chn == "攻擊帶毒")
		{
			return std::to_string(r->AttackWithPoison);
		}
		else if (chn == "左右互搏")
		{
			return std::to_string(r->AttackTwice);
		}
		else if (chn == "聲望")
		{
			return std::to_string(r->Fame);
		}
		else if (chn == "資質")
		{
			return std::to_string(r->IQ);
		}
		else if (chn == "修煉物品")
		{
			return std::to_string(r->PracticeItem);
		}
		else if (chn == "修煉點數")
		{
			return std::to_string(r->ExpForItem);
		}
		else if (chn == "所會武功1")
		{
			return std::to_string(r->MagicID[0]);
		}
		else if (chn == "所會武功2")
		{
			return std::to_string(r->MagicID[1]);
		}
		else if (chn == "所會武功3")
		{
			return std::to_string(r->MagicID[2]);
		}
		else if (chn == "所會武功4")
		{
			return std::to_string(r->MagicID[3]);
		}
		else if (chn == "所會武功5")
		{
			return std::to_string(r->MagicID[4]);
		}
		else if (chn == "所會武功6")
		{
			return std::to_string(r->MagicID[5]);
		}
		else if (chn == "所會武功7")
		{
			return std::to_string(r->MagicID[6]);
		}
		else if (chn == "所會武功8")
		{
			return std::to_string(r->MagicID[7]);
		}
		else if (chn == "所會武功9")
		{
			return std::to_string(r->MagicID[8]);
		}
		else if (chn == "所會武功10")
		{
			return std::to_string(r->MagicID[9]);
		}
		else if (chn == "武功等級1")
		{
			return std::to_string(r->MagicLevel[0]);
		}
		else if (chn == "武功等級2")
		{
			return std::to_string(r->MagicLevel[1]);
		}
		else if (chn == "武功等級3")
		{
			return std::to_string(r->MagicLevel[2]);
		}
		else if (chn == "武功等級4")
		{
			return std::to_string(r->MagicLevel[3]);
		}
		else if (chn == "武功等級5")
		{
			return std::to_string(r->MagicLevel[4]);
		}
		else if (chn == "武功等級6")
		{
			return std::to_string(r->MagicLevel[5]);
		}
		else if (chn == "武功等級7")
		{
			return std::to_string(r->MagicLevel[6]);
		}
		else if (chn == "武功等級8")
		{
			return std::to_string(r->MagicLevel[7]);
		}
		else if (chn == "武功等級9")
		{
			return std::to_string(r->MagicLevel[8]);
		}
		else if (chn == "武功等級10")
		{
			return std::to_string(r->MagicLevel[9]);
		}
		else if (chn == "攜帶物品1")
		{
			return std::to_string(r->TakingItem[0]);
		}
		else if (chn == "攜帶物品2")
		{
			return std::to_string(r->TakingItem[1]);
		}
		else if (chn == "攜帶物品3")
		{
			return std::to_string(r->TakingItem[2]);
		}
		else if (chn == "攜帶物品4")
		{
			return std::to_string(r->TakingItem[3]);
		}
		else if (chn == "攜帶物品數量1")
		{
			return std::to_string(r->TakingItemCount[0]);
		}
		else if (chn == "攜帶物品數量2")
		{
			return std::to_string(r->TakingItemCount[1]);
		}
		else if (chn == "攜帶物品數量3")
		{
			return std::to_string(r->TakingItemCount[2]);
		}
		else if (chn == "攜帶物品數量4")
		{
			return std::to_string(r->TakingItemCount[3]);
		}
		return "";
	}
};