#include "Common.h"
#include "Save.h"
#include "win32-specific\icon\include\iconv.h"
#include <queue>

using namespace cocostudio;
using namespace ui;
using namespace std;

//std::vector<Texture2D*> MyTexture2D::TextureVector;
std::vector<MyTexture2D> MyTexture2D::tex[MyTexture2D::MaxType][MyTexture2D::MaxRole];
int CommonScene::Mx, CommonScene::My;
CommonScene::Towards CommonScene::towards;
//EventKeyboard::KeyCode CommonScene::keypressing;
MyPoint::MyPoint()
{
	x = y = step = g = h = f = Gx = Gy = 0;
	parent = NULL;
	for (int i = 0; i < 4; i++){
		child[i] = NULL;
	}
	towards = LeftDown;
}

void MyPoint::delTree(MyPoint* root)
{
		if (root == NULL) {				// 空子树，直接返回
			return;
		}
		else {							// 非空子树
			delTree(root->child[0]);	// 删除子树
			delTree(root->child[1]);
			delTree(root->child[2]);
			delTree(root->child[3]);
			delete(root);                 // 释放根节点
		}
}


MyTexture2D* MyTexture2D::addImage(const char* path, Type type, int index, float offsetX, float offsetY, int num, bool load, bool antiAlias )
{
	//if (type != 2){ num = 1; }
    MyTexture2D* self = &tex[type][num][index];
    //int type_i = type;
    self->_offset = Point(offsetX, offsetY);
    self->_load = load;
    self->_index = index;
    self->_s = path;
    self->_type = type;
	self->_num = num;

    Texture2D* t = nullptr, * t0 = nullptr;
    if (load)
    {
        char s[255];
        sprintf(s, "%s/%d.png", path, index);
        if (FileUtils::getInstance()->isFileExist(s))
        {
            t = Director::getInstance()->getTextureCache()->addImage(s);
            self->_texture.push_back(t);
			if (!antiAlias) 
				t->setAliasTexParameters();
        }
        else
        {
            for (int i = 0; i < 20; i++)
            {
                sprintf(s, "%s/%d_%d.png", path, index, i);
                if (FileUtils::getInstance()->isFileExist(s))
                {
                    t = Director::getInstance()->getTextureCache()->addImage(s);
                    self->_texture.push_back(t);
					if (!antiAlias)
						t->setAliasTexParameters();
                }
                else
                {
                    break;
                }
            }
        }
    }
    //tex[type][index]._texture = t0;
    tex[type][num][index]._self = self;
    self->_totalFrame = self->_texture.size();
    return self;
}

void MyTexture2D::setToSprite(Sprite* s, Point p, int frame, float scale, bool antiAlias)
{
    auto t = this;
    if (t == nullptr)
    {
        return;
    }
    if (!t->_load)
    {
		addImage(t->_s, t->_type, t->_index, t->getOffset().x, t->getOffset().y, t->_num, true );
    }
    if (t->_totalFrame <= 0)
    {
        return;
    }
    auto tex = t->getTexture(frame);
    if (tex)
    {
		s->setPosition(p.x - t->getOffset().x, - p.y + t->getOffset().y);
        s->setVisible(true);
        s->setTexture(tex);
		if (antiAlias)
			tex->setAntiAliasTexParameters();
        auto r = Rect::ZERO;
        r.size = tex->getContentSize();
        s->setTextureRect(r);
		s->setScale(scale);
    }
}

int CommonScene::numberOfSprites = 0;

void CommonScene::ReadFile(const char* tfile, void* data, int length)
{
    Data s1 = FileUtils::getInstance()->getDataFromFile(tfile);
    memcpy(data, s1.getBytes(), std::min(length, int(s1.getSize())));
    s1.clear();
}

Point CommonScene::GetPositionOnScreen(int x, int y, int CenterX, int CenterY)
{
    Point p;
	p.x = -(x - CenterX) * singleScene_X + (y - CenterY) * singleScene_X + Center_X;
	p.y = ((x - CenterX) * singleScene_Y + (y - CenterY) * singleScene_Y - Center_Y);
    return p;
}

Point CommonScene::getMapPoint(int x, int y, int CenterX, int CenterY)
{
	Point p;
	p.x = -(x - CenterX) * singleMapScene_X + (y - CenterY) * singleMapScene_X + Center_X;
	p.y = ((x - CenterX) * singleMapScene_Y + (y - CenterY) * singleMapScene_Y - Center_Y);
	return p;
}


void CommonScene::addNewSpriteIntoVector(std::vector<Sprite*> &v, int order, Node* node)
{
    auto s = Sprite::create();
    s->setAnchorPoint(Point(0, 1));
    if (node)
    {
        node->addChild(s, order);
    }
    else
    {
		s->retain();
    }
    v.push_back(s);
}

void CommonScene::loadTexture(const char* path, MyTexture2D::Type type, int length, int num)
{
    //读入贴图
	auto file = FileUtils::getInstance();
	   auto offset = new short[length][2];
	char s[255];
	sprintf(s, "%s/index.ka", path);
	if (length < 1000){
		length = file->getFileSize(s) / sizeof(short) / 2;
	}	
	ReadFile(s, offset, length * sizeof(short) * 2);
    MyTexture2D::tex[type][num].resize(length);
    for (int i = 0; i < length; i++)
    {
// 		char fileIn[30];
// 		file->addSearchPath(path);
// 		printf(fileIn,"d%.png",i);
// 		if (file->isFileExist(fileIn)){
// 		}
		MyTexture2D::addImage(path, type, i, offset[i][0], offset[i][1], num);
    }
    delete[] offset;
}


int CommonScene::calNumberOfSprites()
{
    if (numberOfSprites <= 0)
    {
        int s = 0;
        int widthregion = Center_X / 36 + 3;
        int sumregion = Center_Y / 9 + 2;
        for (int sum = -sumregion; sum <= sumregion + 15; sum++)
        {
            for (int i = -widthregion; i <= widthregion; i++)
            {
                s++;
            }
        }
        numberOfSprites = s;
    }
    return numberOfSprites;
}

void CommonScene::keyPressed(EventKeyboard::KeyCode keyCode, Event* event)
{
	if (keyCode == EventKeyboard::KeyCode::KEY_ESCAPE)
	{
		keypressing = EventKeyboard::KeyCode::KEY_NONE;
	}
	else
	{
		keypressing = keyCode;
	}
}

void CommonScene::keyReleased(EventKeyboard::KeyCode keyCode, Event* event)
{
	if (keyCode == EventKeyboard::KeyCode::KEY_ESCAPE)
	{
		keypressing = EventKeyboard::KeyCode::KEY_ESCAPE;
	}
	else
	{
		keypressing = EventKeyboard::KeyCode::KEY_NONE;
	}
}
void CommonScene::mouseDown(EventMouse eventMouse, Event *event){}
void CommonScene::mouseUp(EventMouse eventMouse, Event *event){}
void CommonScene::mouseScroll(EventMouse eventMouse, Event *event){}
void CommonScene::mouseMove(EventMouse eventMouse, Event *event){}
bool CommonScene::touchBegan(Touch *touch, Event *event)
{
	return true;
}
void CommonScene::touchEnded(Touch *touch, Event *event){}
void CommonScene::touchCancelled(Touch *touch, Event *event){}
void CommonScene::touchMoved(Touch *touch, Event *event){}


void CommonScene::checkTimer(float dt)
{
	drawCount++;
	this->checkTimer2();
}

void CommonScene::checkTimer2()
{
}

void CommonScene::replaceLocation()
{
	Mx = m_BasicData[0].Mx;
	My = m_BasicData[0].My;
	towards = (Towards)m_BasicData[0].MFace;
}

void CommonScene::drawWords(std::string *content, int size,int length,Vec2 *vec)
{
	for (int i = 0; i <length; i++)
	{
		std::string contentu = a2u(content[i].c_str());
		log("trant s  %s, %s", content[i].c_str(), contentu.c_str());
		auto text = Text::create(contentu, "fonts/chinese.ttf", size);
		Color3B color;
		color.r = 144;
		color.b = 238;
		color.g = 160;
		text->setColor(color);
		text->setPosition(vec[i]);
		text->setAnchorPoint(Vec2(0, 0));
		TextLayer->addChild(text);
		text->setTag(i+40); //文字tag 从40开始；
	}
	

}

void CommonScene::showRoleAbility(int x, int y, int num)
{
	for (int i = 0; i < num; i++)
	{
		auto sprite = Sprite::create("ui/beizhan/6.png");
		sprite->setPosition(Vec2(x + i * 20, y));
		SpriteLayer->addChild(sprite);
	}
}
void  CommonScene::cleanRoleVerticaldRawing()
{

	SpriteLayer->removeAllChildren();
}
void  CommonScene::showFiveDimensional(int x,int y,int num)
{
	
	int r[5];
	int offest = 5;
	double max = 220.0;
	auto draw = DrawNode::create();

	Vec2 point1[5];

	int R = 70;
	Vec2 point2[5];
	Vec2 point = Vec2(x, y); // 初始坐标  矩阵是变幻，向量是坐标 -bttt
	Vec2 p;
	double PI = 3.1415926;
	for (int i = 0; i < 5; i++)
	{
		p = Vec2(cos((90 + i * 72) / 180.0*PI), sin((90 + i * 72) / 180.0*PI));
		point2[i] = point + R*p;
	}

	for (int i = 0; i < 4; i++)
	{
		
		for (int i = 0; i < 5; i++)
		{
			p = Vec2(cos((90 + i * 72) / 180.0*PI), sin((90 + i * 72) / 180.0*PI));
			point1[i] = point + R*p;
		}
		draw->drawPoly(point1, 5, true, Color4F::WHITE);
		R -= 20;
	}
	

	for (int i = 0; i < 5; i++)
	{
		draw->drawLine(point1[i], point2[i], Color4F::WHITE);
	}
	R = 70;
	r[0] = (m_Character[num].Fencing / max)*R + offest;
	r[1] = (m_Character[num].Boxing / max)*R + offest; 
	r[2] = (m_Character[num].Knife / max)*R + offest;
	r[3] = (m_Character[num].SpecialSkill / max)*R + offest;
	r[4] = (m_Character[num].Shader / max)*R + offest;
	for (int i = 0; i < 5; i++)
	{
		p = Vec2(cos((90 + i * 72) / 180.0*PI), sin((90 + i * 72) / 180.0*PI));
		point1[i] = point + r[i] * p;
	}
	draw->drawSolidPoly(point1, 5, Color4F::BLUE);

	point = Vec2(x, y-10);
	R = 75;
	for (int i = 0; i < 5; i++)
	{
		p = Vec2(cos((90 + i * 72) / 180.0*PI), sin((90 + i * 72) / 180.0*PI));
		point2[i] = point + R*p;
	}
	point2[0].x = point2[0].x - 18;
	point2[1].x =point2[1].x - 30;
	point2[2].x =point2[2].x - 30;
	point2[2].y = point2[2].y - 7;
	string str1[5];
	str1[0] = "剑法";
	str1[1] = "刀法";
	str1[2] = "拳法";
	str1[3] = "兵器";
	str1[4] = "暗器";
	drawWords(str1, 20, 5, point2);
	fivedimensionalLayer->addChild(draw);
}


void  CommonScene::cleanFiveDimensional()
{

	fivedimensionalLayer->removeAllChildren();
}




void CommonScene::showRoleVerticaldRawing(int x, int y, int num, int Opacity, float ScaleX, float ScaleY)
{
	string str = "rolehead/" + to_string(num) + ".png";
	auto sprite = Sprite::create(str);
	sprite->setAnchorPoint(Vec2(0, 0));
	sprite->setPosition(Vec2(x, y));
	sprite->setScaleX(ScaleX);
	sprite->setScaleY(ScaleY);
	sprite->setOpacity(Opacity);
	SpriteLayer->addChild(sprite);
}

void CommonScene::myRunAction(string str,int begin ,int end)
{
	Node* node = CSLoader::createNode(str);
	SpriteLayer->addChild(node);
	auto action = CSLoader::createTimeline(str);

	node->runAction(action);
   action->gotoFrameAndPlay(begin, end, true);
   

	node->setAnchorPoint(Vec2(0, 0));
	node->setPosition(Vec2(0, 0));
}

void CommonScene::cleanAllWords()
{
	TextLayer->removeAllChildren();
}
void CommonScene::cleanSlectWords(int num)
{
	TextLayer->removeChildByTag(40+num);
}

int CommonScene::code_convert(const char *from_charset, const char *to_charset, const char *inbuf, size_t inlen, char *outbuf, size_t outlen)
{
	iconv_t cd;
	const char *temp = inbuf;
	const char **pin = &temp; 
	char **pout = &outbuf;
	memset(outbuf, 0, outlen);
	cd = iconv_open(to_charset, from_charset);
	if (cd == 0) return -1;
	if (iconv(cd, pin, &inlen, pout, &outlen) == -1) return -1;
	iconv_close(cd);
	return 0;
}

std::string CommonScene::a2u(const char *inbuf)
{
	size_t inlen = strlen(inbuf);
	char * outbuf = new char[inlen * 2 + 2];
	string strRet;
	if (code_convert("cp936", "utf-8", inbuf, inlen, outbuf, inlen * 2 + 2) == 0)
	{
		strRet = outbuf;
	}
	delete[] outbuf;
	return strRet;
}

void CommonScene::FindWay(int Mx, int My, int Fx, int Fy)
{}

bool CommonScene::checkIsOutLine(int x, int y)
{ return false; }

bool CommonScene::checkIsBuilding(int x, int y)
{ return false; }

void CommonScene::initData(int scenenum)
{
// 	curSData = &save.SData[scenenum];
// 	curDData = &SaveGame::getInstance()->DData[scenenum];
// 	curRScene = &SaveGame::getInstance()->RScene[scenenum];
// 	for (int i = 0; i < SaveGame::MaxTeamMember; i++)
// 	{
// 		teamList[i] = &SaveGame::getInstance()->RBasic_Data.TeamList[i];
// 	}
// 
// 	for (int i = 0; i < SaveGame::MaxRole; i++)
// 	{
// 		role[i] = &SaveGame::getInstance()->Rrole[i];
// 	}
}

void CommonScene::initData()
{
// 	for (int i = 0; i < SaveGame::MaxRole; i++)
// 	{
// 		role[i] = &SaveGame::getInstance()->Role[i];
// 	}
}

int CommonScene::CallFace(int x1, int y1, int x2, int y2)
{
	int d1, d2, dm;
	d1 = x2 - x1;
	d2 = y2 - y1;
	dm = abs(d1) - abs(d2);
	if ((d1 != 0) || (d2 != 0))
	{
		if (dm >= 0)
		{
			if (d1 < 0)
			{
				return 0;
			}
			else
			{
				return 3;
			}
		}
		else{
			if (d2 < 0){
				return 2;
			}
			else
			{
				return 1;
			}
		}
	}
}

int MyPoint::Heuristic(int Fx, int Fy) {						//manhattan估价函数
	h = (abs(x - Fx) + abs(y - Fy)) * 10;
	return h;
}
