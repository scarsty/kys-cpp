#pragma once

class battle
{
public:
	battle();
	~battle();

	enum
	{
		MaxBRoleNum = 42,						//战场最大人数
		BLayerCount = 2,						//数据文件存储地图数据层数
		BLayerCountNew = 8,						//战场需要地图层数
		BSceneMaxX = 64,						//战场最大X坐标
		BSceneMaxY = 64,						//战场最大Y坐标
	};

	struct TWarSta
	{
		short battleNum;
		char name[10];
		short battleMap, exp, battleMusic; //9
		short mate[12], autoMate[12], mate_x[12], mate_y[12]; //56
		short enemy[30], enemy_x[30], enemy_y[30]; //146
		short boutEvent, operationEvent;
		short getKongFu[3], getItems[3];//154
		short getMoney;
	};

	//short AddResourse[10], Connection[10];
	struct TBattleRole
	{
		int rnum, Team, Y, X, Face, Dead, Step, Acted;
		int Pic, ShowNumber, showgongji, showfangyu, szhaoshi, Progress, round, speed; //15
		int ExpGot, Auto, Show, Wait, frozen, killed, Knowledge, LifeAdd, Zhuanzhu, pozhao, wanfang; //24
		//AddAtt, AddDef, AddSpd, addfenfa,addjingzhun,AddStep, AddDodge,addtime: Smallint; //31 luke删除，改为每回合衰减
		int zhuangtai[14];
		int lzhuangtai[10];
		int Address[51];
	};

	struct SceneBData
	{
		short Data[BLayerCountNew][BSceneMaxX][BSceneMaxY];
	};
	
	std::vector<SceneBData> bData;													//define battle data
	
	//vector<DataNew> bDataNew;													//define battle data
	std::vector<TBattleRole> BRole;									//define battle role
	std::vector<TWarSta> warSta;

	struct FactionBackup
	{
		int len, captain;
		int TMenber[10];
	};
	
	int* offset;


	int maxBattleScene;
	
	static battle bBattle;

	static battle* getInstance()
	{
		return &bBattle;
	}
	
	bool loadSta();
	bool initSta(int currentBattle);
};

