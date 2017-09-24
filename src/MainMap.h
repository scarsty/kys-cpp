#pragma once
#include <stack>
#include "Scene.h"
#include <queue>

class Cloud : public Base
{
public:
    Cloud() {}
    virtual ~Cloud() {}

    enum CloudTowards
    {
        Left = 0,
        Right = 1,
        Up = 2,
        Down = 3,
    };

    Point position;
    float speed;


    const int maxX = 17280;
    const int maxY = 8640;
    enum { numTexture = 10 };
    int num;

    void initRand();
    void setPositionOnScreen(int x, int y, int Center_X, int Center_Y);
    void changePosition();

};

class MainMap : public Scene
{
public:
    MainMap();
    ~MainMap();
    //RenderTexture* tex[8];
    int const static maxX = 480;
    int const static maxY = 480;

    typedef short MapArray[maxX][maxY];
    static MapArray Earth, Surface, Building, BuildX, BuildY, Entrance;

    void divide2(MapArray& ma);

    static bool _readed;
    //int walk[30][2];
    std::stack<Point> wayQue;  //栈(路径栈)

    //std::vector<Sprite*> EarthS, SurfaceS, BuildingS, CloudS;

    int cloudX, cloudY;
    int step = 0;
    int manPicture;
    int restTime = 0;                    //停止操作的时间
    int cloud_restTime = 0;              //云消失的时间
    int const offset_manPic = 2501;      //初始主角图偏移量
    int const num_manPic = 7;            //单向主角图张数
    int const offset_restPic = 2529;     //主角休息图偏移量
    int const num_restPic = 6;           //单向休息图张数
    int const begin_restTime = 200;      //开始休息的时间
    int const each_pictrueTime = 15;     //休息图切换间隔
    int const cloudSize = 240;           //云朵宽度
    int const static tag_mainLayer = 1;  //主层编号
    int const static tag_wordLayer = 1;  //文字层编号


    bool isEsc = false;  //是否已打开系统菜单

    Cloud::CloudTowards cloudTowards = Cloud::Left;

    std::vector<Cloud*> cloudVector;

    //SceneData* curRScene;
    void draw() override;


    void init() override;

    void dealEvent(BP_Event& e) override;
    void walk(int x, int y, Towards t);
    void cloudMove();
    void getEntrance();

    virtual bool checkIsBuilding(int x, int y);
    bool checkIsWater(int x, int y);
    virtual bool checkIsOutLine(int x, int y);
    bool checkIsOutScreen(int x, int y);
    void getMousePosition(int _x, int _y);
    bool canWalk(int x, int y);
    bool checkIsEntrance(int x, int y);
    virtual void FindWay(int Mx, int My, int Fx, int Fy);
    void stopFindWay();
private:
    short Mx = 240, My = 240;
};
