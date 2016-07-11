#ifndef __Common_H__
#define __Common_H__

#include "cocos2d.h"
#include "battle.h"
#include "cocostudio/CocoStudio.h"
#include "cocos-ext.h"
#include "ui/CocosGUI.h"

USING_NS_CC;

using namespace std;

class MyPoint : public Point
{
public:
	int x, y, step;
	int g, h, f;
	int Gx, Gy;
	
	enum Towards
	{
		LeftUp = 0,
		RightUp = 1,
		LeftDown = 2,
		RightDown = 3,
	}towards;
	MyPoint* parent;
	MyPoint* child[4];

	void delTree(MyPoint*);

	bool lessthan (const MyPoint *myPoint) const
	{
		return f > myPoint->f;												//重载比较运算符
	}
	int Heuristic(int Fx, int Fy);
	MyPoint();
	~MyPoint()
	{
		//delete parent;
	}
};

class Compare
{
public:
	bool operator () (MyPoint *point1, MyPoint *point2)
	{
		return point1->lessthan(point2);
	}
};

class MyTexture2D
{
public:
	MyTexture2D(){}
	virtual ~MyTexture2D(){}
	enum Type
	{
		MainMap = 0,
		Scene = 1,
		Battle = 2,
		Cloud = 3,
		MaxType
	};
	//static std::vector<Texture2D*> TextureVector;
	static const int MaxRole = 1000;
	static std::vector<MyTexture2D> tex[MaxType][MaxRole];
	//static std::vector<tex[MaxType]> tex[MaxType];

	Point _offset;
	int _totalFrame = 0;
	int _firstInVector;
	bool _load = false;

	Type _type;
	int _index;
	int _num;
	const char* _s;

	MyTexture2D* _self;
	std::vector<Texture2D*> _texture;

	static MyTexture2D* addImage(const char* path, Type type, int index, float offsetX = 0, float offsetY = 0, int num = 1, bool load = false, bool antiAlias = false);
	
	//这两个是异步读图，不太正常
	/*void addImage(const char* filename)
	{
		Director::getInstance()->getTextureCache()->addImageAsync(filename, CC_CALLBACK_1(MyTextrue2D::callback, this));
	}
	void callback(Texture2D* t)
	{
		this->_texture = t;
	}*/

	//获取图片本身
	static MyTexture2D* getSelfPointer(int list, int num2, int num1 = 1)
	{
		return tex[list][num1][num2]._self;
	}
	//获取偏移
	Point getOffset()
	{
		return _offset;
	}
	//获取纹理
	Texture2D* getTexture(int frame = 0)
	{
		if (_totalFrame <= 0)
			return nullptr;
		return _texture[frame % _totalFrame];
	}
	//获取总帧数
	int getTotalFrame()
	{
		return _totalFrame;
	}

	void setToSprite(Sprite* s, Point p, int frame = 0, float scale = 1, bool antiAlias = false);
};

class CommonScene : public Layer
{
public:

	enum Towards
	{
		LeftUp = 0,
		RightUp = 1,
		LeftDown = 2,
		RightDown = 3,
	};

	enum Where
	{
		None = -1,
		MainMap = 0,
		SubScene = 1
	};

	RenderTexture* renderTex;
	Sprite* MainRole;
	Sprite* BackGround;
	Layer* TextLayer;
	Layer* SpriteLayer;
	Layer* fivedimensionalLayer;

	const int MaxMainMapCoord = 479;
	const int MaxSceneCoord = 63;

	static const int Center_X = 512;									
	static const int Center_Y = 272;
	static const int singleScene_X = 18;								//小图块大小X
	static const int singleScene_Y = 9;							//小图块大小Y
	static const int singleMapScene_X = 18;							//地面小图块大小X
	static const int singleMapScene_Y = 9;							//地面小图块大小Y
	static int Mx, My;

	int minStep;														//起点(Mx,My),终点(Fx,Fy),最少移动次数minStep
	int Msx, Msy;
	int widthregion = Center_X / 36 + 3;
	int sumregion = Center_Y / 9 + 2;		
	bool isMenuOn = 0;

	static Towards towards;

/*	SaveGame &save = SaveGame::save;									//引用save*/
// 	SaveGame::TSceneSData* curSData;									//定义场景数据
// 	SaveGame::TSceneDData* curDData;									//定义D数据
// 	SaveGame::TScene* curRScene;										//当前场景
// 	SaveGame::TRole* role[SaveGame::MaxRole];							//定义人物对象
// 	short* teamList[SaveGame::MaxTeamMember];							//队伍列表	
// 	SaveGame::TMagic* magic[SaveGame::MaxMagic];						//定义武功数据

	int drawCount = 0;

	static int numberOfSprites;

	EventKeyboard::KeyCode keypressing;
	EventMouse::MouseEventType mouseClick;

	virtual void keyPressed(EventKeyboard::KeyCode keyCode, Event *event);
	virtual void keyReleased(EventKeyboard::KeyCode keyCode, Event *event);
	virtual void mouseDown(EventMouse eventMouse, Event *event);
	virtual void mouseUp(EventMouse eventMouse, Event *event);
	virtual void mouseScroll(EventMouse eventMouse, Event *event);
	virtual void mouseMove(EventMouse eventMouse, Event *event);
	virtual bool touchBegan(Touch *touch, Event *event);
	virtual void touchEnded(Touch *touch, Event *event);
	virtual void touchCancelled(Touch *touch, Event *event);
	virtual void touchMoved (Touch *touch, Event *event);

	void initData(int scenenum);
	void initData();

	CommonScene()
	{ 
		renderTex = RenderTexture::create(Center_X * 2, Center_Y * 2);
		BackGround = Sprite::create();
		BackGround->addChild(renderTex);
		SpriteLayer = Layer::create();
		this->addChild(SpriteLayer, 15);
		fivedimensionalLayer = Layer::create();
		this->addChild(fivedimensionalLayer,10);

		BackGround->setTexture(renderTex->getSprite()->getTexture());
		this->addChild(BackGround, -1);
		BackGround->setPosition(Center_X, Center_Y);
		TextLayer = Layer::create();
		this->addChild(TextLayer, 15);
		schedule(schedule_selector(CommonScene::checkTimer), 0.025, kRepeatForever, 0.025);
	}

	virtual ~CommonScene(){}

	Point GetPositionOnScreen(int x, int y, int CenterX, int CenterY);
	Point getMapPoint(int x, int y, int CenterX, int CenterY);
	void ReadFile(const char* tfile, void* data, int length);

	void addNewSpriteIntoVector(std::vector<Sprite*> &v, int order = 0, Node* node = nullptr);
	void loadTexture(const char* path, MyTexture2D::Type type, int length, int num = 1);

	void checkTimer(float dt);
	void menuOn(){
		isMenuOn = 1;
	}
	void menuClose(){
		isMenuOn = 0;
	}

	virtual void checkTimer2();

	int calNumberOfSprites();
	void replaceLocation();
	void drawWords(std::string *content,int size, int length, Vec2 *vec);
	void cleanAllWords();
	void cleanSlectWords(int num); //tag 从40开始
	void showRoleAbility(int x, int y, int num);                         // 显示人物能力等级（星星）
	void showRoleVerticaldRawing(int x, int y, int num, int Opacity, float ScaleX, float ScaleY);                  // 显示人物立绘
	void cleanRoleVerticaldRawing();                                                                               //清除人物立绘                                 
	void myRunAction(string str, int begin, int end);                                                              //显示动画
	void showFiveDimensional(int x, int y, int num);                                                                                    //显示五维图 
	void cleanFiveDimensional();                                                                                    //清除五维图

	virtual void FindWay(int Mx, int My, int Fx, int Fy);
	virtual bool checkIsOutLine(int x, int y);
	virtual bool checkIsBuilding(int x, int y);

	int CallFace(int x1, int y1, int x2, int y2);

	int code_convert(const char *from_charset, const char *to_charset, const char *inbuf, size_t inlen, char *outbuf, size_t outlen);

	std::string a2u(const char *inbuf);

};


#endif 