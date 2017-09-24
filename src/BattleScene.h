#pragma once
#include "BattleData.h"
#include "Scene.h"
#include "Point.h"
#include <stack>

using namespace std;

class BattleScene : public Scene
{
public:
    BattleScene();
    ~BattleScene();

	virtual void init() override;
    virtual void draw() override;
    virtual void dealEvent(BP_Event& e) override;

    static const int m_nMaxBRoleSelect = 6;       //战斗选人最大人数
    int m_nMaxBRoleNum = 42;                      //最大战场参战人数

    const int m_nbattleSpeed = 0.025;             //战斗速度
    int m_nBx = 31, m_nBy = 31;                                //当前位置
    int m_nAx, m_nAy;                                //目标位置
    int m_nlinex[480 * 480], m_nliney[480 * 480];
    int m_nmanPicture;
    int m_nstep = 0;
    int m_ncurA;
    int m_ncounter = 0;                           // 计数器解决人物选择逻辑问题
    int m_nxx = 317;                              // 人物信息界面的x
    int m_nyy = 382;                              // 人物信息界面的y
    enum
    {
        kindOfRole = 7                         //人物信息界面的属性种类
    };
    string m_strList[BattleScene::kindOfRole];    // 人物信息界面的属性（默认姓名，攻击，防御，轻功）
    int m_nList = 0;                          // 人物信息界面的列表位置
    int m_nOffset_BRolePic = 1;                   //单向战斗图张数
    int m_nBRoleAmount = 0;                       //战场人数
    int m_nMods = 0;                              //战斗模式
    string m_strMenuString[m_nMaxBRoleSelect + 2];     //战斗人物选单
    int m_nMax0;                                  //最大人数
    int m_nMaxspeed;                              //最大步数

    int m_nBStatus;
    bool m_bisBattle;
    int m_nWhere;
    int m_ncurRoleNum;                            //当前人物序号
    int m_nBattleList[m_nMaxBRoleSelect];            //参战人员列表
    int m_nResultofBattle[m_nMaxBRoleSelect];        //战斗选人结果
    //MenuItemFont* item[maxBRoleSelect + 2];                                 //选人控件
    bool m_bSlectOfresult[m_nMaxBRoleSelect + 2];    //选项状态
    int m_nbattleNum;                             //当前战斗号
    int m_nbattleSceneNum = 0;                        //当前战斗场景号
    int m_ncurMagic;                              //当前武功

    //std::vector<Sprite*> EarthS, BuildS, AirS, EventS;
    std::stack<Point> m_wayQue;                       //栈(路径栈)


    BattleData& Battle = BattleData::m_bBattle;  //引用battle
    //vector<battle::TBattleRole> BRole;   //定义战斗人员
    std::vector<BattleSceneData>& m_vcBattleSceneData = BattleData::getInstance()->m_vcBattleSceneData;
    std::vector<BattleRole>& m_vcBattleRole = BattleData::getInstance()->m_vcBattleRole;
    std::vector<BattleInfo>& m_vcBattleInfo = BattleData::getInstance()->m_vcBattleInfo;
	//std::vector<Character>& Rrole = Save::getInstance()->m_Character;

    void walk(int x, int y, Towards t);

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
    void showSlectMenu(string* str, int x);                               // 参战人物信息
    void showBattleMenu(int x, int y);                                    // 战斗界面

    void initMultiMenu();

    void getMousePosition(Point* point);
    virtual void FindWay(int Mx, int My, int Fx, int Fy);

    int CallFace(int x1, int y1, int x2, int y2);

    inline int calBlockTurn(int x, int y, int layer)
    {
        return 4 * (128 * (x + y) + x) + layer;
    }

    //void menuCloseCallback(Ref* pSender);
    //void menuCloseCallback1(Ref* pSender);
    bool battle(int battlenum, int getexp, int mods, int id, int maternum, int enemyrnum);
    void setInitState(int& n0);
    void calMoveAbility();
    void reArrangeBRole();
    int getRoleSpeed(int rnum, bool Equip);
    int getGongtiLevel(int rnum);
    float power(float base, float Exponent);
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