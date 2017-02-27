#include "Event.h"
#include "MainMap.h"
#include "SubScene.h"


EventManager EventManager::eventManager;
EventManager::EventManager()
{

}


EventManager::~EventManager()
{
}

bool EventManager::callEvent(int num)
{
    Head::inEvent = true;
    int eventId = -1;

    for (int i = 0; i < eventCount; i++)
    {
        if (eventData.at(i).checkId(num))
        {
            eventId = i;
            break;
        }
    }
    if (eventId < 0)
    {
        return false;
    }
    const std::vector<Operation>* operation = eventData.at(eventId).getOperation();
    int parLen;
    EventInstruct event;

    int p = 0;
    int length = 100;
    while (p < length)
    {
        p += EventInstruct::funcs[operation->at(p).num](p);
    }

}

bool EventManager::initEventData()
{

    int idxLen = 0;         //存储文件相关
    int* offset;
    eventData.resize(0);
    for (int num1 = 0; num1 <= config::EventFolderNum; num1++)
    {
        char path[20];
        sprintf(path, "%s%4d", "event/", num1); //事件文件夹结构，第一层4位数目录，第二层3位数文件，文件内还有4位数的事件号
        //std::string path = StringUtils::format("event/%.4d", num1);
        if (_access(path, 0) == -1)
        {
            _mkdir(path);
        }
        for (int num2 = 0; num2 <= config::EventFileNum; num2++)
        {
            char filename1[30], filename2[30];
            //path1 = path + StringUtils::format("/%.3d", num2);
            sprintf(filename1, "%s%3d%s", path, num2, ".idx");

            unsigned char* Eidx;
            File::readFile(filename1, Eidx, idxLen);
            //cocos2d::Data Eidx = FileUtils::getInstance()->getDataFromFile(filename1);
            offset = new int[idxLen / 4 + 1];
            offset = new int[idxLen / 4 + 1];
            *offset = 0;
            memcpy(offset + 1, Eidx, idxLen);

            sprintf(filename2, "%s%3d%s", path, num2, ".grp");
            unsigned char* Egrp;
            int EgrpLen;
            //这儿开始，11.30
            //cocos2d::Data Egrp = FileUtils::getInstance()->getDataFromFile(filename2);
            File::readFile(filename2, Egrp, EgrpLen);
            unsigned char* Data;
            Data = new unsigned char[EgrpLen];
            memcpy(Data, Egrp, EgrpLen);
            for (int j = 0; offset[j] < EgrpLen; j++)
            {
                eventCount++;
                eventData.resize(eventCount);
                int length = offset[j + 1] - offset[j];
                eventData.at(eventCount - 1).setId(num1 * 10000000 + num2 * 10000 + j);
                eventData.at(eventCount - 1).arrByOperation(Data + offset[j], length);
            }
        }
    }
    return true;
}


void EventData::setId(int num)
{
    _id = num;
}
bool EventData::checkId(int num)
{
    return (_id == num);
}
const vector<Operation>* EventData::getOperation()
{
    return &_operations;
}


void EventData::arrByOperation(unsigned char* Data, int len)
{
    int add0 = 0;
    int count = 0;
    _operations.resize(0);

    short* D = (short*)Data;
    len /= 2;
    while (add0 < len)
    {
        count++;
        Operation tmp;
        tmp.num = *((D + add0));
        int add1 = getOperationLen(tmp.num);
        for (int j = 0; j < add1; j++)
        {
            tmp.par.push_back(*(D + add0 + j));
        }
        _operations.push_back(tmp);
        add0 += add1;
    }
}
int EventData::getOperationLen(int num)
{
    switch (num)
    {
    case -1:
    {
        return 1;
        break;
    }
    case 0:
    {
        return 1;
        break;
    }
    case 1:
    {
        return 4;
        break;
    }
    case 2:
    {
        return 3;
        break;
    }
    case 3:
    {
        return 21;
        break;
    }
    case 4:
    {
        return 4;
        break;
    }
    case 5:
    {
        return 3;
        break;
    }
    case 6:
    {
        return 5;
        break;
    }
    case 7: // 获取随机舞台
    {
        return 7;
        break;
    }
    case 8:
    {
        return 2;
        break;
    }
    case 9:
    {
        return 3;
        break;
    }
    case 10:
    {
        return 2;
        break;
    }
    case 11:
    {
        return 3;
        break;
    }
    case 12:
    {
        return 1;
        break;
    }
    case 13:
    {
        return 1;
        break;
    }
    case 14:
    {
        return 1;
        break;
    }
    case 15:
    {
        return 1;
        break;
    }
    case 16:
    {
        return 4;
        break;
    }
    case 17:
    {
        return 6;
        break;
    }
    case 18:
    {
        return 4;
        break;
    }
    case 19:
    {
        return 3;
        break;
    }
    case 20:
    {
        return 3;
        break;
    }
    case 21:
    {
        return 2;
        break;
    }
    case 22:
    {
        return 1;
        break;
    }
    case 23:
    {
        return 3;
        break;
    }
    case 24:
    {
        return 1;
        break;
    }
    case 25:
    {
        return 5;
        break;
    }
    case 26:
    {
        return 6;
        break;
    }
    case 27:
    {
        return 4;
        break;
    }
    case 28:
    {
        return 6;
        break;
    }
    case 29:
    {
        return 6;
        break;
    }
    case 30:
    {
        return 5;
        break;
    }
    case 31:
    {
        return 4;
        break;
    }
    case 32:
    {
        return 3;
        break;
    }
    case 33:
    {
        return 5;
        break;
    }
    case 34:
    {
        return 3;
        break;
    }
    case 35:
    {
        return 5;
        break;
    }
    case 36:
    {
        return 4;
        break;
    }
    case 37:
    {
        return 3;
        break;
    }
    case 38:
    {
        return 5;
        break;
    }
    case 39:
    {
        return 2;
        break;
    }
    case 40:
    {
        return 2;
        break;
    }
    case 41:
    {
        return 4;
        break;
    }
    case 42:
    {
        return 3;
        break;
    }
    case 43:
    {
        return 4;
        break;
    }
    case 44:
    {
        return 7;
        break;
    }
    case 45:
    {
        return 3;
        break;
    }
    case 46:
    {
        return 3;
        break;
    }
    case 47:
    {
        return 3;
        break;
    }
    case 48:
    {
        return 3;
        break;
    }
    case 49:
    {
        return 3;
        break;
    }
    case 50:
    {
        return 8;
        break;
    }
    case 51:
    {
        return 4;
        break;
    }
    case 52:
    {
        return 1;
        break;
    }
    case 53:
    {
        return 1;
        break;
    }
    case 54:
    {
        return 1;
        break;
    }
    case 55:
    {
        return 5;
        break;
    }
    case 56:
    {
        return 3;
        break;
    }
    case 57:
    {
        return 1;
        break;
    }
    case 58:
    {
        return 1;
        break;
    }
    case 59:
    {
        return 1;
        break;
    }
    case 60:
    {
        return 6;
        break;
    }
    case 61:
    {
        return 3;
        break;
    }
    case 62:
    {
        return 1;
        break;
    }
    case 63:
    {
        return 3;
        break;
    }
    case 64:
    {
        return 2;
        break;
    }
    case 65:
    {
        return 1;
        break;
    }
    case 66:
    {
        return 2;
        break;
    }
    case 67:
    {
        return 2;
        break;
    }
    case 68:
    {
        return 8;
        break;
    }
    case 69:
    {
        return 4;
        break;
    }
    case 70:
    {
        return 3;
        break;
    }
    case 71:
    {
        return 4;
        break;
    }

    case 73:
    {
        return 3;
        break;
    }
    case 74:
    {
        return 8;
        break;
    }
    case 75:
    {
        return 2;
        break;
    }
    case 76:
    {
        return 1;
        break;
    }
    case 77:
    {
        return 3;
        break;
    }
    case 78:
    {
        return 5;
        break;
    }
    case 79:
    {
        return 2;
        break;
    }
    case 80:
    {
        return 3;
        break;
    }
    case 81:
    {
        return 5;
        break;
    }

    case 82:
    {
        return 4;
        break;
    }
    case 83:
    {
        return 6;
        break;
    }
    case 84:
    {
        return 3;
        break;
    }
    case 85:
    {
        return 6;
        break;
    }
    case 86:
    {
        return 4;
        break;
    }
    case 87:
    {
        return 6;
        break;
    }
    case 88:
    {
        return 3;
        break;
    }
    case 89:
    {
        return 6;
        break;
    }
    case 90:
    {
        return 3;
        break;
    }
    case 91:
    {
        return 3;
        break;
    }
    case 92:
    {
        return 8;
        break;
    }
    case 93:
    {
        return 6;
        break;
    }
    case 94:
    {
        return 3;
        break;
    }
    case 95:
    {
        return 4;
        break;
    }
    case 96:
    {
        return 3;
        break;
    }
    case 97:
    {
        return 4;
        break;
    }
    case 98:
    {
        return 2;
        break;
    }
    case 99:
    {
        return 3;
        break;
    }
    case 100:
    {
        return 1;
        break;
    }
    case 101:
    {
        return 1;
        break;
    }
    case 102:
    {
        return 1;
        break;
    }
    case 103:
    {
        return 1;
        break;
    }
    case 104:
    {
        return 2;
        break;
    }
    case 105:
    {
        return 3;
        break;
    }
    case 106:
    {
        return 2;
        break;
    }
    case 107:
    {
        return 4;
        break;
    }
    case 108:
    {
        return 2;
        break;
    }
    case 109:
    {
        return 20;
        break;
    }
    case 110:
    {
        return 3;
        break;
    }
    case 111:
    {
        return 6;
        break;
    }
    case 112:
    {
        return 5;
        break;
    }
    case 113:
    {
        return 3;
        break;
    }
    case 114:
    {
        return 4;
        break;
    }
    case 115:
    {
        return 3;
        break;
    }
    case 116:
    {
        return 5;
        break;
    }
    case 117:
    {
        return 2;
        break;
    }
    case 118:
    {
        return 7;
        break;
    }
    case 119:
    {
        return 9;
        break;
    }
    case 120:
    {
        return 3;
        break;
    }
    case 121:
    {
        return 2;
        break;
    }
    case 122: // 读取当前事件触发人.
    {
        return 5;
        break;
    }
    case 123: // 直接将人物放到地图
    {
        return 26;
        break;
    }
    case 124: // 增加或修改任务提示
    {
        return 6;
        break;
    }
    case 125: // 下场战斗增加人员
    {
        return 4;
        break;
    }
    case 126: // 比賽
    {
        return 5;
        break;
    }
    case 127: // 进入堆
    {
        return 3;
        break;
    }
    case 128: // 弹出堆
    {
        return 3;
        break;
    }
    case 129: // 清空堆
    {
        return 1;
        break;
    }
    case 130: // 新增自动检测任务
    {
        return 11;
        break;
    }
    case 131: // 修改任务
    {
        return 9;
        break;
    }
    case 132: // 武功融合
    {
        return 5;
        break;
    }
    case 133: //搜索人物标签
    {
        return 9;
        break;
    }
    case 134: //搜索门派
    {
        return 14;
        break;
    }
    case 135: //设置时代
    {
        return 4;
        break;
    }
    case 136: //判断时代
    {
        return 5;
        break;
    }
    case 137: //获得时间
    {
        return 2;
        break;
    }
    case 138: //获得标签
    {
        return 4;
        break;
    }
    case 139: //判断标签
    {
        return 6;
        break;
    }
    case 140: //大排名
    {
        return 6;
        break;
    }
    }
}
void EventInstruct::XXX()
{

}

#define EVENT_FUNC(name) funcs.push_back(std::bind(&EventInstruct::name, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5))

EventInstruct::EventInstruct()
{
    funcs.clear();
    EVENT_FUNCS;
}

#undef EVENT_FUNC

#define EVENT_FUNC(name) int EventInstruct::name(int p1,int p2, int p3, int p4,int p5)

EVENT_FUNC(clear_screen)
{
    return 1;
}

EVENT_FUNC(talk)
{
    return 4;
}

EVENT_FUNC(isZhangMen) 
{
	int i;
	if (rnum <= -10) {
		rnum = getJueSe(rnum);
	}

	Result: = jump2;
	if (snum == -2) {
		for (int i = 0; i < length(rmenpai) - 1; i++) {
			if(rmenpai[i].zmr == rnum){
				return jump1;
			}
		}
	}
	else {
		if (snum == -1) {
			snum = CurScene;
		}
	}
	if (rmenpai[Rscene[snum].menpai].zmr = rnum) {
		return jump1;
	}

	return 78;
}



#undef EVENT_FUNC

vector<EventFunc> EventInstruct::funcs;
