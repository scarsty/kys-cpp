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
	const std::vector<Operation> *operation = eventData.at(eventId).getOperation();
	int parLen;
	EventInstruct event;
	for (int i = 0; i < operation->size(); i++)
	{
		switch (operation->at(i).num)
		{
		case -1:
		{
			parLen = 1;
			event.XXX();
			break;
		}
		case 0:
		{
			parLen = 1;
			event.XXX();
			break;
		}
		case 1:
		{
			parLen = 4;
			event.XXX();
			break;
		}
		case 2:
		{
			parLen = 3;
			event.XXX();
			break;
		}
		case 3:
		{
			parLen = 21;
			event.XXX();
			break;
		}
		case 4:
		{
			parLen = 4;
			event.XXX();
			break;
		}
		case 5:
		{
			parLen = 3;
			event.XXX();
			break;
		}
		case 6:
		{
			parLen = 5;
			event.XXX();
			break;
		}
		case 7: // 获取随机舞台
		{
			parLen = 7;
			event.XXX();
			break;
		}
		case 8:
		{
			parLen = 2;
			event.XXX();
			break;
		}
		case 9:
		{
			parLen = 3;
			event.XXX();
			break;
		}
		case 10:
		{
			parLen = 2;
			event.XXX();
			break;
		}
		case 11:
		{
			parLen = 3;
			event.XXX();
			break;
		}
		case 12:
		{
			parLen = 1;
			event.XXX();
			break;
		}
		case 13:
		{
			parLen = 1;
			event.XXX();
			break;
		}
		case 14:
		{
			parLen = 1;
			event.XXX();
			break;
		}
		case 15:
		{
			parLen = 1;
			event.XXX();
			break;
		}
		case 16:
		{
			parLen = 4;
			event.XXX();
			break;
		}
		case 17:
		{
			parLen = 6;
			event.XXX();
			break;
		}
		case 18:
		{
			parLen = 4;
			event.XXX();
			break;
		}
		case 19:
		{
			parLen = 3;
			event.XXX();
			break;
		}
		case 20:
		{
			parLen = 3;
			event.XXX();
			break;
		}
		case 21:
		{
			parLen = 2;
			event.XXX();
			break;
		}
		case 22:
		{
			parLen = 1;
			event.XXX();
			break;
		}
		case 23:
		{
			parLen = 3;
			event.XXX();
			break;
		}
		case 24:
		{
			parLen = 1;
			event.XXX();
			break;
		}
		case 25:
		{
			parLen = 5;
			event.XXX();
			break;
		}
		case 26:
		{
			parLen = 6;
			event.XXX();
			break;
		}
		case 27:
		{
			parLen = 4;
			event.XXX();
			break;
		}
		case 28:
		{
			parLen = 6;
			event.XXX();
			break;
		}
		case 29:
		{
			parLen = 6;
			event.XXX();
			break;
		}
		case 30:
		{
			parLen = 5;
			event.XXX();
			break;
		}
		case 31:
		{
			parLen = 4;
			event.XXX();
			break;
		}
		case 32:
		{
			parLen = 3;
			event.XXX();
			break;
		}
		case 33:
		{
			parLen = 5;
			event.XXX();
			break;
		}
		case 34:
		{
			parLen = 3;
			event.XXX();
			break;
		}
		case 35:
		{
			parLen = 5;
			event.XXX();
			break;
		}
		case 36:
		{
			parLen = 4;
			event.XXX();
			break;
		}
		case 37:
		{
			parLen = 3;
			event.XXX();
			break;
		}
		case 38:
		{
			parLen = 5;
			event.XXX();
			break;
		}
		case 39:
		{
			parLen = 2;
			event.XXX();
			break;
		}
		case 40:
		{
			parLen = 2;
			event.XXX();
			break;
		}
		case 41:
		{
			parLen = 4;
			event.XXX();
			break;
		}
		case 42:
		{
			parLen = 3;
			event.XXX();
			break;
		}
		case 43:
		{
			parLen = 4;
			event.XXX();
			break;
		}
		case 44:
		{
			parLen = 7;
			event.XXX();
			break;
		}
		case 45:
		{
			parLen = 3;
			event.XXX();
			break;
		}
		case 46:
		{
			parLen = 3;
			event.XXX();
			break;
		}
		case 47:
		{
			parLen = 3;
			event.XXX();
			break;
		}
		case 48:
		{
			parLen = 3;
			event.XXX();
			break;
		}
		case 49:
		{
			parLen = 3;
			event.XXX();
			break;
		}
		case 50:
		{
			parLen = 8;
			event.XXX();
			break;
		}
		case 51:
		{
			parLen = 4;
			event.XXX();
			break;
		}
		case 52:
		{
			parLen = 1;
			event.XXX();
			break;
		}
		case 53:
		{
			parLen = 1;
			event.XXX();
			break;
		}
		case 54:
		{
			parLen = 1;
			event.XXX();
			break;
		}
		case 55:
		{
			parLen = 5;
			event.XXX();
			break;
		}
		case 56:
		{
			parLen = 3;
			event.XXX();
			break;
		}
		case 57:
		{
			parLen = 1;
			event.XXX();
			break;
		}
		case 58:
		{
			parLen = 1;
			event.XXX();
			break;
		}
		case 59:
		{
			parLen = 1;
			event.XXX();
			break;
		}
		case 60:
		{
			parLen = 6;
			event.XXX();
			break;
		}
		case 61:
		{
			parLen = 3;
			event.XXX();
			break;
		}
		case 62:
		{
			parLen = 1;
			event.XXX();
			break;
		}
		case 63:
		{
			parLen = 3;
			event.XXX();
			break;
		}
		case 64:
		{
			parLen = 2;
			event.XXX();
			break;
		}
		case 65:
		{
			parLen = 1;
			event.XXX();
			break;
		}
		case 66:
		{
			parLen = 2;
			event.XXX();
			break;
		}
		case 67:
		{
			parLen = 2;
			event.XXX();
			break;
		}
		case 68:
		{
			parLen = 8;
			event.XXX();
			break;
		}
		case 69:
		{
			parLen = 4;
			event.XXX();
			break;
		}
		case 70:
		{
			parLen = 3;
			event.XXX();
			break;
		}
		case 71:
		{
			parLen = 4;
			event.XXX();
			break;
		}

		case 73:
		{
			parLen = 3;
			event.XXX();
			break;
		}
		case 74:
		{
			parLen = 8;
			event.XXX();
			break;
		}
		case 75:
		{
			parLen = 2;
			event.XXX();
			break;
		}
		case 76:
		{
			parLen = 1;
			event.XXX();
			break;
		}
		case 77:
		{
			parLen = 3;
			event.XXX();
			break;
		}
		case 78:
		{
			parLen = 5;
			event.XXX();
			break;
		}
		case 79:
		{
			parLen = 2;
			event.XXX();
			break;
		}
		case 80:
		{
			parLen = 3;
			event.XXX();
			break;
		}
		case 81:
		{
			parLen = 5;
			event.XXX();
			break;
		}

		case 82:
		{
			parLen = 4;
			event.XXX();
			break;
		}
		case 83:
		{
			parLen = 6;
			event.XXX();
			break;
		}
		case 84:
		{
			parLen = 3;
			event.XXX();
			break;
		}
		case 85:
		{
			parLen = 6;
			event.XXX();
			break;
		}
		case 86:
		{
			parLen = 4;
			event.XXX();
			break;
		}
		case 87:
		{
			parLen = 6;
			event.XXX();
			break;
		}
		case 88:
		{
			parLen = 3;
			event.XXX();
			break;
		}
		case 89:
		{
			parLen = 6;
			event.XXX();
			break;
		}
		case 90:
		{
			parLen = 3;
			event.XXX();
			break;
		}
		case 91:
		{
			parLen = 3;
			event.XXX();
			break;
		}
		case 92:
		{
			parLen = 8;
			event.XXX();
			break;
		}
		case 93:
		{
			parLen = 6;
			event.XXX();
			break;
		}
		case 94:
		{
			parLen = 3;
			event.XXX();
			break;
		}
		case 95:
		{
			parLen = 4;
			event.XXX();
			break;
		}
		case 96:
		{
			parLen = 3;
			event.XXX();
			break;
		}
		case 97:
		{
			parLen = 4;
			event.XXX();
			break;
		}
		case 98:
		{
			parLen = 2;
			event.XXX();
			break;
		}
		case 99:
		{
			parLen = 3;
			event.XXX();
			break;
		}
		case 100:
		{
			parLen = 1;
			event.XXX();
			break;
		}
		case 101:
		{
			parLen = 1;
			event.XXX();
			break;
		}
		case 102:
		{
			parLen = 1;
			event.XXX();
			break;
		}
		case 103:
		{
			parLen = 1;
			event.XXX();
			break;
		}
		case 104:
		{
			parLen = 2;
			event.XXX();
			break;
		}
		case 105:
		{
			parLen = 3;
			event.XXX();
			break;
		}
		case 106:
		{
			parLen = 2;
			event.XXX();
			break;
		}
		case 107:
		{
			parLen = 4;
			event.XXX();
			break;
		}
		case 108:
		{
			parLen = 2;
			event.XXX();
			break;
		}
		case 109:
		{
			parLen = 20;
			event.XXX();
			break;
		}
		case 110:
		{
			parLen = 3;
			event.XXX();
			break;
		}
		case 111:
		{
			parLen = 6;
			event.XXX();
			break;
		}
		case 112:
		{
			parLen = 5;
			event.XXX();
			break;
		}
		case 113:
		{
			parLen = 3;
			event.XXX();
			break;
		}
		case 114:
		{
			parLen = 4;
			event.XXX();
			break;
		}
		case 115:
		{
			parLen = 3;
			event.XXX();
			break;
		}
		case 116:
		{
			parLen = 5;
			event.XXX();
			break;
		}
		case 117:
		{
			parLen = 2;
			event.XXX();
			break;
		}
		case 118:
		{
			parLen = 7;
			event.XXX();
			break;
		}
		case 119:
		{
			parLen = 9;
			event.XXX();
			break;
		}
		case 120:
		{
			parLen = 3;
			event.XXX();
			break;
		}
		case 121:
		{
			parLen = 2;
			event.XXX();
			break;
		}
		case 122: // 读取当前事件触发人.
		{
			parLen = 5;
			event.XXX();
			break;
		}
		case 123: // 直接将人物放到地图
		{
			parLen = 26;
			event.XXX();
			break;
		}
		case 124: // 增加或修改任务提示
		{
			parLen = 6;
			event.XXX();
			break;
		}
		case 125: // 下场战斗增加人员
		{
			parLen = 4;
			event.XXX();
			break;
		}
		case 126: // 比賽
		{
			parLen = 5;
			event.XXX();
			break;
		}
		case 127: // 进入堆
		{
			parLen = 3;
			event.XXX();
			break;
		}
		case 128: // 弹出堆
		{
			parLen = 3;
			event.XXX();
			break;
		}
		case 129: // 清空堆
		{
			parLen = 1;
			event.XXX();
			break;
		}
		case 130: // 新增自动检测任务
		{
			parLen = 11;
			event.XXX();
			break;
		}
		case 131: // 修改任务
		{
			parLen = 9;
			event.XXX();
			break;
		}
		case 132: // 武功融合
		{
			parLen = 5;
			event.XXX();
			break;
		}
		case 133: //搜索人物标签
		{
			parLen = 9;
			event.XXX();
			break;
		}
		case 134: //搜索门派
		{
			parLen = 14;
			event.XXX();
			break;
		}
		case 135: //设置时代
		{
			parLen = 4;
			event.XXX();
			break;
		}
		case 136: //判断时代
		{
			parLen = 5;
			event.XXX();
			break;
		}
		case 137: //获得时间
		{
			parLen = 2;
			event.XXX();
			break;
		}
		case 138: //获得标签
		{
			parLen = 4;
			event.XXX();
			break;
		}
		case 139: //判断标签
		{
			parLen = 6;
			event.XXX();
			break;
		}
		case 140: //大排名
		{
			parLen = 6;
			event.XXX();
			break;
		}
		}
	}
}

bool EventManager::initEventData()
{

	int idxLen = 0;			//存储文件相关
	int* offset;
	eventData.resize(0);
	for (int num1 = 0; num1 <= config::EventFolderNum; num1++)
	{
		char path[20];
		sprintf(path, "%s%4d", "event/", num1);	//事件文件夹结构，第一层4位数目录，第二层3位数文件，文件内还有4位数的事件号
		//std::string path = StringUtils::format("event/%.4d", num1);  
		if (_access(path, 0) == -1) {
			_mkdir(path);
		}
		for (int num2 = 0; num2 <= config::EventFileNum; num2++)
		{
			char filename1[30], filename2[30];
			//path1 = path + StringUtils::format("/%.3d", num2);
			sprintf(filename1, "%s%3d%s", path, num2,".idx");

			unsigned char* Eidx;
			File::readFile(filename1, Eidx, idxLen);
			//cocos2d::Data Eidx = FileUtils::getInstance()->getDataFromFile(filename1);
			offset = new int[idxLen / 4 + 1];
			offset = new int[idxLen / 4 + 1];
			*offset = 0;
			memcpy(offset + 1, Eidx, idxLen);

			sprintf(filename2, "%s%3d%s", path, num2, ".grp");
			unsigned char* Egrp;
			//这儿开始，11.30
			//cocos2d::Data Egrp = FileUtils::getInstance()->getDataFromFile(filename2);
			if (!Egrp.isNull())
			{
				int Egrplen = Egrp.getSize();
				unsigned char *Data;
				Data = new unsigned char[Egrplen];
				memcpy(Data, Egrp.getBytes(), Egrplen);
				for (int j = 0; offset[j] < Egrplen; j++)
				{
					eventCount++;
					eventData.resize(eventCount);
					int length = offset[j + 1] - offset[j];
					eventData.at(eventCount - 1).setId(num1 * 10000000 + num2 * 10000 + j);
					eventData.at(eventCount - 1).arrByOperation(Data + offset[j], length);
				}
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


void EventData::arrByOperation(unsigned char *Data, int len)
{
	int add0 = 0;
	int count = 0;
	_operations.resize(0);
	
	short *D = (short *)Data;
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