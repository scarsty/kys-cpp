#pragma once

class config
{
public:
	config();
	~config();

	enum
	{
		_Rtypecount = 10,
		SLayerCount = 6,
		SceneMaxX = 64,
		SceneMaxY = 64,
		PerSceneMaxEvent = 400,							//单场景最大事件数	
		MAXScene = 200,									//最大场景数
		MAX_ITEM_AMOUNT = 400,							//最大物品数
		MaxRole = 1000,									//最大人物数
		Maxfaction = 200,								//最大门派数
		MaxZhaoShi = 1000,								//最大招式数
		MaxMagic = 1000,								//最大武功数
		MaxTeamMember = 6,								//最大队伍人员数
		MaxEquipNum = 5,								//最大装备数量
		MaxEventNum = 3000,								//最大事件数
		MaxEventFactor = 50,							//单事件最大参数数
		EventFolderNum = 1,								//事件文件夹数	
		EventFileNum = 1								//单事件文件夹事件文件数

	};

};

