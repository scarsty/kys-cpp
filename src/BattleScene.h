#pragma once

using namespace std;
 
class BattleScene : public Scene
{
public:
	BattleScene();
	~BattleScene();

    virtual void draw() override {}
    virtual void dealEvent(BP_Event& e) override {}

	static const int maxBRoleSelect = 6;		//战斗选人最大人数
	int MaxBRoleNum = 42;						//最大战场参战人数	
//	int Sx, Sy;
	const int battleSpeed = 0.025;				//战斗速度	
	int Bx, By;									//当前位置
	int Ax, Ay;									//目标位置
	int linex[480 * 480], liney[480 * 480];
	int manPicture;
	int step = 0;
	int curA;
	int counter = 0;                    // 计数器解决人物选择逻辑问题
	int xx = 317;                       // 人物信息界面的x
	int yy = 382;                       // 人物信息界面的y
	enum
	{
		kindOfRole = 7				  //人物信息界面的属性种类
	};
	string s_list[BattleScene::kindOfRole]; // 人物信息界面的属性（默认姓名，攻击，防御，轻功）
	int num_list = 0;                   // 人物信息界面的列表位置
	int offset_BRolePic = 1;				//单向战斗图张数
	int BRoleAmount = 0;				//战场人数
	int mods = 0;						//战斗模式
	string menuString[maxBRoleSelect+2];				//战斗人物选单
	int max0;						    //最大人数
	int maxspeed;						//最大步数

	int BStatus;
	bool isBattle;
	int WHERE;                          
	int curRoleNum;									//当前人物序号
	int BattleList[maxBRoleSelect];                  //参战人员列表
	int ResultofBattle[maxBRoleSelect];              //战斗选人结果
	MenuItemFont *item[maxBRoleSelect+2];              //选人控件
	bool SlectOfresult[maxBRoleSelect+2];                  //选项状态
	int battleNum;										//当前战斗号
	int sceneNum;										//当前战斗场景号
	int curMagic;										//当前武功

	std::vector<Sprite*> EarthS, BuildS, AirS, EventS;
	stack<MyPoint> wayQue;												//栈(路径栈)


	battle &Battle = battle::bBattle;										//引用battle
 	vector<battle::TBattleRole> BRole;							//定义战斗人员
	
	void Draw();

	virtual bool init(int scenenum);

	static BattleScene* create(int scenenum);

	static Scene* createScene(int scenenum);

	//void keyPressed(EventKeyboard::KeyCode keyCode, Event *event);
	//void keyReleased(EventKeyboard::KeyCode keyCode, Event *event);
	void Walk(int x, int y, Towards t);

	virtual void keyPressed(EventKeyboard::KeyCode keyCode, Event *event);
	virtual void keyReleased(EventKeyboard::KeyCode keyCode, Event *event);
	bool touchBegan(Touch *touch, Event *event);
	void touchEnded(Touch *touch, Event *event);
	void touchCancelled(Touch *touch, Event *event);
	void touchMoved(Touch *touch, Event *event);

	void checkTimer2();
	void callEvent(int x, int y);
	bool canWalk(int x, int y);
	virtual bool checkIsBuilding(int x, int y);
	virtual bool checkIsOutLine(int x, int y);
	bool checkIsHinder(int x, int y);
	bool checkIsEvent(int x, int y);
	bool checkIsFall(int x, int y);
	bool checkIsExit(int x, int y);
	bool checkIsOutScreen(int x, int y);

	void initData(int battlenum);
	bool initBattleData();
	bool initBattleRoleState();
	bool autoInBattle();

	void autoSetMagic(int rnum);
	int selectTeamMembers();
	void ShowMultiMenu(int max0, int menu);
	void showSlectMenu(string *str,int x);                               // 参战人物信息
	void showBattleMenu(int x,int y);                                    // 战斗界面
	
	void initMultiMenu();
	
	void loadBattleTiles();
	void getMousePosition(Point *point);
	virtual void FindWay(int Mx, int My, int Fx, int Fy);

	int CallFace(int x1, int y1, int x2, int y2);
	int CallFace(int num1, int num2);

	inline int calBlockTurn(int x, int y, int layer)
	{
		return 4 * (128 * (x + y) + x) + layer;
	}

	void menuCloseCallback(Ref* pSender);
	void menuCloseCallback1(Ref* pSender);
	bool battle(int battlenum, int getexp, int mods, int id, int maternum, int enemyrnum);
	void setInitState(int &n0);
	void calMoveAbility();
	void reArrangeBRole();
	int getRoleSpeed(int rnum, bool Equip);
	int getGongtiLevel(int rnum);
	float power(float Base, float Exponent);
	void moveRole(int bnum);
	void calCanSelect(int bnum, int mode, int step);
	void seekPath2(int x, int y, int step, int myteam, int mode);
	void moveAmination(int bnum);
	bool selectAim(int bnum, int step, int mods);
	void battleMainControl();
	void battleMainControl(int mods, int id);
	//void Draw(float dt);
	void moveAminationStep(float dt);
	void attack(int bum);
};