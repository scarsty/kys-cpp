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
    //EventInstruct event;

    int p = 0;
    int length = 100;
    while (p < length)
    {
        int instruct = operation->at(p).num;
        switch (num)
        {
        case -1:
        {
            break;
        }
        case 0:
        {
//            clear();
            break;
        }
        case 1:
        {
            break;
        }
        case 2:
        {
            break;
        }
        case 3:
        {
            break;
        }
        case 4:
        {
            break;
        }
        case 5:
        {
            break;
        }
        case 6:
        {
            break;
        }
        case 7: // 获取随机舞台
        {
            break;
        }
        case 8:
        {
            break;
        }
        case 9:
        {
            break;
        }
        case 10:
        {
            break;
        }
        case 11:
        {
            break;
        }
        case 12:
        {
            break;
        }
        case 13:
        {
            break;
        }
        case 14:
        {
            break;
        }
        case 15:
        {
            break;
        }
        case 16:
        {
            break;
        }
        case 17:
        {
            break;
        }
        case 18:
        {
            break;
        }
        case 19:
        {
            break;
        }
        case 20:
        {
            break;
        }
        case 21:
        {
            break;
        }
        case 22:
        {
            break;
        }
        case 23:
        {
            break;
        }
        case 24:
        {
            break;
        }
        case 25:
        {
            break;
        }
        case 26:
        {
            break;
        }
        case 27:
        {
            break;
        }
        case 28:
        {
            break;
        }
        case 29:
        {
            break;
        }
        case 30:
        {
            break;
        }
        case 31:
        {
            break;
        }
        case 32:
        {
            break;
        }
        case 33:
        {
            break;
        }
        case 34:
        {
            break;
        }
        case 35:
        {
            break;
        }
        case 36:
        {
            break;
        }
        case 37:
        {
            break;
        }
        case 38:
        {
            break;
        }
        case 39:
        {
            break;
        }
        case 40:
        {
            break;
        }
        case 41:
        {
            break;
        }
        case 42:
        {
            break;
        }
        case 43:
        {
            break;
        }
        case 44:
        {
            break;
        }
        case 45:
        {
            break;
        }
        case 46:
        {
            break;
        }
        case 47:
        {
            break;
        }
        case 48:
        {
            break;
        }
        case 49:
        {
            break;
        }
        case 50:
        {
            break;
        }
        case 51:
        {
            break;
        }
        case 52:
        {
            break;
        }
        case 53:
        {
            break;
        }
        case 54:
        {
            break;
        }
        case 55:
        {
            break;
        }
        case 56:
        {
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
            break;
        }
        case 60:
        {
            break;
        }
        case 61:
        {
            break;
        }
        case 62:
        {
            break;
        }
        case 63:
        {
            break;
        }
        case 64:
        {
            break;
        }
        case 65:
        {
            break;
        }
        case 66:
        {
            break;
        }
        case 67:
        {
            break;
        }
        case 68:
        {
            break;
        }
        case 69:
        {
            break;
        }
        case 70:
        {
            break;
        }
        case 71:
        {
            break;
        }

        case 73:
        {
            break;
        }
        case 74:
        {
            break;
        }
        case 75:
        {
            break;
        }
        case 76:
        {
            break;
        }
        case 77:
        {
            break;
        }
        case 78:
        {
            break;
        }
        case 79:
        {
            break;
        }
        case 80:
        {
            break;
        }
        case 81:
        {
            break;
        }

        case 82:
        {
            break;
        }
        case 83:
        {
            break;
        }
        case 84:
        {
            break;
        }
        case 85:
        {
            break;
        }
        case 86:
        {
            return 4;
            break;
        }
        case 87:
        {
            break;
        }
        case 88:
        {
            break;
        }
        case 89:
        {
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
            break;
        }
        case 94:
        {
            break;
        }
        case 95:
        {
            break;
        }
        case 96:
        {
            return 3;
            break;
        }
        case 97:
        {
            break;
        }
        case 98:
        {
            break;
        }
        case 99:
        {
            break;
        }
        case 100:
        {
            break;
        }
        case 101:
        {
            break;
        }
        case 102:
        {
            break;
        }
        case 103:
        {
            break;
        }
        case 104:
        {
            break;
        }
        case 105:
        {
            break;
        }
        case 106:
        {
            break;
        }
        case 107:
        {
            break;
        }
        case 108:
        {
            break;
        }
        case 109:
        {
            break;
        }
        case 110:
        {
            break;
        }
        case 111:
        {
            break;
        }
        case 112:
        {
            break;
        }
        case 113:
        {
            break;
        }
        case 114:
        {
            break;
        }
        case 115:
        {
            break;
        }
        case 116:
        {
            break;
        }
        case 117:
        {
            break;
        }
        case 118:
        {
            break;
        }
        case 119:
        {
            break;
        }
        case 120:
        {
            break;
        }
        case 121:
        {
            break;
        }
        case 122: // 读取当前事件触发人.
        {
            break;
        }
        case 123: // 直接将人物放到地图
        {
            break;
        }
        case 124: // 增加或修改任务提示
        {
            break;
        }
        case 125: // 下场战斗增加人员
        {
            break;
        }
        case 126: // 比賽
        {
            break;
        }
        case 127: // 进入堆
        {
            break;
        }
        case 128: // 弹出堆
        {
            break;
        }
        case 129: // 清空堆
        {
            break;
        }
        case 130: // 新增自动检测任务
        {
            break;
        }
        case 131: // 修改任务
        {
            break;
        }
        case 132: // 武功融合
        {
            break;
        }
        case 133: //搜索人物标签
        {
            break;
        }
        case 134: //搜索门派
        {
            break;
        }
        case 135: //设置时代
        {
            break;
        }
        case 136: //判断时代
        {
            break;
        }
        case 137: //获得时间
        {
            break;
        }
        case 138: //获得标签
        {
            break;
        }
        case 139: //判断标签
        {
            break;
        }
        case 140: //大排名
        {
            break;
        }
        }
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
    std::vector<int> ret = { 1, 4, 3, 21, 4, 3, 5, 7, 2, 3, 2, 3, 1, 1, 1, 1, 4, 6, 4, 3, 3, 2, 1, 3, 1, 5, 6, 4, 6, 6, 5, 4, 3, 5, 3, 5, 4, 3, 5, 2, 2, 4, 3, 4, 7, 3, 3, 3, 3, 3, 8, 4, 1, 1, 1, 5, 3, 1, 1, 1, 6, 3, 1, 3, 2, 1, 2, 2, 8, 4, 3, 4, 3, 8, 2, 1, 3, 5, 2, 3, 5, 4, 6, 3, 6, 4, 6, 3, 6, 3, 3, 8, 6, 3, 4, 3, 4, 2, 3, 1, 1, 1, 1, 2, 3, 2, 4, 2, 20, 3, 6, 5, 3, 4, 3, 5, 2, 7, 9, 3, 2, 5, 26, 6, 4, 5, 3, 3, 1, 11, 9, 5, 9, 14, 4, 5, 2, 4, 6, 6 };
    return ret[num];
}
//void EventInstruct::XXX()
//{
//
//}


void EventManager::clear() {
	
}


void EventManager::talk() {

}

int EventManager::getGongtiLevel(int rnum, int gongti) {
	if (rnum >= 0 && gongti >= -1) {
		if (Save::getInstance()->m_Magic[Save::getInstance()->m_Character[rnum].LMagic[gongti]].MaxLevel > Save::getInstance()->m_Character[rnum].MagLevel[gongti] / 100) {
			return Save::getInstance()->m_Character[rnum].MagLevel[gongti] / 100;
		}
		else {
			return Save::getInstance()->m_Magic[Save::getInstance()->m_Character[rnum].LMagic[gongti]].MaxLevel;
		}
	}
	else {
		return 0;
	}
}

int EventManager::getRoleSpeed(int rnum, bool equip = false) {
	short speed = Save::getInstance()->m_Character[rnum].Speed;
	if (Save::getInstance()->m_Character[rnum].GongTi > -1) {
		int magicnum = Save::getInstance()->m_Character[rnum].LMagic[Save::getInstance()->m_Character[rnum].GongTi];
		speed += Save::getInstance()->m_Magic[magicnum].AddSpd[getGongtiLevel(rnum, Save::getInstance()->m_Character[rnum].GongTi)];
	}
	if (equip) {
		for (int i = 0; i < config::MaxEquipNum; i++) {
			if (Save::getInstance()->m_Character[rnum].Equip[i] >= 0) {
				speed += Save::getInstance()->m_Item[Save::getInstance()->m_Character[rnum].Equip[i]].AddSpeed;
			}
		}
	}
	speed = speed * 100 / (100 + Save::getInstance()->m_Character[rnum].Wounded + Save::getInstance()->m_Character[rnum].Poison);
	return speed;	
}

int EventManager::getRoleDefence(int rnum, bool equip = false) {
	short defence = Save::getInstance()->m_Character[rnum].Defence;
	if (Save::getInstance()->m_Character[rnum].GongTi > -1) {
		int magicnum = Save::getInstance()->m_Character[rnum].LMagic[Save::getInstance()->m_Character[rnum].GongTi];
		defence += Save::getInstance()->m_Magic[magicnum].AddDef[getGongtiLevel(rnum, Save::getInstance()->m_Character[rnum].GongTi)];
	}
	if (equip) {
		for (int i = 0; i < config::MaxEquipNum; i++) {
			if (Save::getInstance()->m_Character[rnum].Equip[i] >= 0) {
				defence += Save::getInstance()->m_Item[Save::getInstance()->m_Character[rnum].Equip[i]].AddDefence;
			}
		}
	}
	defence = defence * 100 / (100 + Save::getInstance()->m_Character[rnum].Wounded + Save::getInstance()->m_Character[rnum].Poison);
	return defence;
}

int EventManager::getRoleAttack(int rnum, bool equip = false) {
	short attack = Save::getInstance()->m_Character[rnum].Attack;
	if (Save::getInstance()->m_Character[rnum].GongTi > -1) {
		int magicnum = Save::getInstance()->m_Character[rnum].LMagic[Save::getInstance()->m_Character[rnum].GongTi];
		attack += Save::getInstance()->m_Magic[magicnum].AddAtt[getGongtiLevel(rnum, Save::getInstance()->m_Character[rnum].GongTi)];
	}
	if (equip) {
		for (int i = 0; i < config::MaxEquipNum; i++) {
			if (Save::getInstance()->m_Character[rnum].Equip[i] >= 0) {
				attack += Save::getInstance()->m_Item[Save::getInstance()->m_Character[rnum].Equip[i]].AddAttack;
			}
		}
	}
	attack = attack * 100 / (100 + Save::getInstance()->m_Character[rnum].Wounded + Save::getInstance()->m_Character[rnum].Poison);
	return attack;
}


int EventManager::getRoleBoxing(int rnum, bool equip = false) {
	short boxing = Save::getInstance()->m_Character[rnum].Boxing;
	if (Save::getInstance()->m_Character[rnum].GongTi > -1) {
		int magicnum = Save::getInstance()->m_Character[rnum].LMagic[Save::getInstance()->m_Character[rnum].GongTi];
		if (getGongtiLevel(rnum, Save::getInstance()->m_Character[rnum].GongTi) == Save::getInstance()->m_Magic[magicnum].MaxLevel) {
			boxing += Save::getInstance()->m_Magic[magicnum].AddBoxing;
		}		
	}
	if (equip) {
		for (int i = 0; i < config::MaxEquipNum; i++) {
			if (Save::getInstance()->m_Character[rnum].Equip[i] >= 0) {
				boxing += Save::getInstance()->m_Item[Save::getInstance()->m_Character[rnum].Equip[i]].AddBoxing;
			}
		}
	}
	return boxing;
}

int EventManager::getRoleFencing(int rnum, bool equip = false) {
	short Fencing = Save::getInstance()->m_Character[rnum].Fencing;
	if (Save::getInstance()->m_Character[rnum].GongTi > -1) {
		int magicnum = Save::getInstance()->m_Character[rnum].LMagic[Save::getInstance()->m_Character[rnum].GongTi];
		if (getGongtiLevel(rnum, Save::getInstance()->m_Character[rnum].GongTi) == Save::getInstance()->m_Magic[magicnum].MaxLevel) {
			Fencing += Save::getInstance()->m_Magic[magicnum].AddFencing;
		}
	}
	if (equip) {
		for (int i = 0; i < config::MaxEquipNum; i++) {
			if (Save::getInstance()->m_Character[rnum].Equip[i] >= 0) {
				Fencing += Save::getInstance()->m_Item[Save::getInstance()->m_Character[rnum].Equip[i]].AddFencing;
			}
		}
	}
	return Fencing;
}

int EventManager::getRoleKnife(int rnum, bool equip = false) {
	short knife = Save::getInstance()->m_Character[rnum].Fencing;
	if (Save::getInstance()->m_Character[rnum].GongTi > -1) {
		int magicnum = Save::getInstance()->m_Character[rnum].LMagic[Save::getInstance()->m_Character[rnum].GongTi];
		if (getGongtiLevel(rnum, Save::getInstance()->m_Character[rnum].GongTi) == Save::getInstance()->m_Magic[magicnum].MaxLevel) {
			knife += Save::getInstance()->m_Magic[magicnum].AddKnife;
		}
	}
	if (equip) {
		for (int i = 0; i < config::MaxEquipNum; i++) {
			if (Save::getInstance()->m_Character[rnum].Equip[i] >= 0) {
				knife += Save::getInstance()->m_Item[Save::getInstance()->m_Character[rnum].Equip[i]].AddKnife;
			}
		}
	}
	return knife;
}

int EventManager::getRoleSpecial(int rnum, bool equip = false) {
	short special = Save::getInstance()->m_Character[rnum].SpecialSkill;
	if (Save::getInstance()->m_Character[rnum].GongTi > -1) {
		int magicnum = Save::getInstance()->m_Character[rnum].LMagic[Save::getInstance()->m_Character[rnum].GongTi];
		if (getGongtiLevel(rnum, Save::getInstance()->m_Character[rnum].GongTi) == Save::getInstance()->m_Magic[magicnum].MaxLevel) {
			special += Save::getInstance()->m_Magic[magicnum].AddSpecial;
		}
	}
	if (equip) {
		for (int i = 0; i < config::MaxEquipNum; i++) {
			if (Save::getInstance()->m_Character[rnum].Equip[i] >= 0) {
				special += Save::getInstance()->m_Item[Save::getInstance()->m_Character[rnum].Equip[i]].AddSpecial;
			}
		}
	}
	return special;
}

int EventManager::getRoleShader(int rnum, bool equip = false) {
	short shader = Save::getInstance()->m_Character[rnum].Shader;
	if (Save::getInstance()->m_Character[rnum].GongTi > -1) {
		int magicnum = Save::getInstance()->m_Character[rnum].LMagic[Save::getInstance()->m_Character[rnum].GongTi];
		if (getGongtiLevel(rnum, Save::getInstance()->m_Character[rnum].GongTi) == Save::getInstance()->m_Magic[magicnum].MaxLevel) {
			shader += Save::getInstance()->m_Magic[magicnum].AddShader;
		}
	}
	if (equip) {
		for (int i = 0; i < config::MaxEquipNum; i++) {
			if (Save::getInstance()->m_Character[rnum].Equip[i] >= 0) {
				shader += Save::getInstance()->m_Item[Save::getInstance()->m_Character[rnum].Equip[i]].AddShader;
			}
		}
	}
	return shader;
}

int EventManager::getRoleDefpoi(int rnum, bool equip = false) {
	short defpoi = Save::getInstance()->m_Character[rnum].DefPoison;
	if (Save::getInstance()->m_Character[rnum].GongTi > -1) {
		int magicnum = Save::getInstance()->m_Character[rnum].LMagic[Save::getInstance()->m_Character[rnum].GongTi];
		if (getGongtiLevel(rnum, Save::getInstance()->m_Character[rnum].GongTi) == Save::getInstance()->m_Magic[magicnum].MaxLevel) {
			defpoi += Save::getInstance()->m_Magic[magicnum].AddDefPoi;
		}
	}
	if (equip) {
		for (int i = 0; i < config::MaxEquipNum; i++) {
			if (Save::getInstance()->m_Character[rnum].Equip[i] >= 0) {
				defpoi += Save::getInstance()->m_Item[Save::getInstance()->m_Character[rnum].Equip[i]].AddDefPoi;
			}
		}
	}
	return defpoi;
}


int EventManager::getRoleAttpoi(int rnum, bool equip = false) {
	short attpoi = Save::getInstance()->m_Character[rnum].AttPoison;
	if (equip) {
		for (int i = 0; i < config::MaxEquipNum; i++) {
			if (Save::getInstance()->m_Character[rnum].Equip[i] >= 0) {
				attpoi += Save::getInstance()->m_Item[Save::getInstance()->m_Character[rnum].Equip[i]].AddAttPoi;
			}
		}
	}
	return attpoi;
}

int EventManager::getRoleUsepoi(int rnum, bool equip = false) {
	short usepoi = Save::getInstance()->m_Character[rnum].AttPoison;
	if (Save::getInstance()->m_Character[rnum].GongTi > -1) {
		int magicnum = Save::getInstance()->m_Character[rnum].LMagic[Save::getInstance()->m_Character[rnum].GongTi];
		if (getGongtiLevel(rnum, Save::getInstance()->m_Character[rnum].GongTi) == Save::getInstance()->m_Magic[magicnum].MaxLevel) {
			usepoi += Save::getInstance()->m_Magic[magicnum].AddUsePoi;
		}
	}
	if (equip) {
		for (int i = 0; i < config::MaxEquipNum; i++) {
			if (Save::getInstance()->m_Character[rnum].Equip[i] >= 0) {
				usepoi += Save::getInstance()->m_Item[Save::getInstance()->m_Character[rnum].Equip[i]].AddUsePoi;
			}
		}
	}
	return usepoi;
}


int EventManager::getRoleMedpoi(int rnum, bool equip = false) {
	short medpoi = Save::getInstance()->m_Character[rnum].AttPoison;
	if (Save::getInstance()->m_Character[rnum].GongTi > -1) {
		int magicnum = Save::getInstance()->m_Character[rnum].LMagic[Save::getInstance()->m_Character[rnum].GongTi];
		if (getGongtiLevel(rnum, Save::getInstance()->m_Character[rnum].GongTi) == Save::getInstance()->m_Magic[magicnum].MaxLevel) {
			medpoi += Save::getInstance()->m_Magic[magicnum].AddMedPoi;
		}
	}
	if (equip) {
		for (int i = 0; i < config::MaxEquipNum; i++) {
			if (Save::getInstance()->m_Character[rnum].Equip[i] >= 0) {
				medpoi += Save::getInstance()->m_Item[Save::getInstance()->m_Character[rnum].Equip[i]].AddMedPoison;
			}
		}
	}
	return medpoi;
}

int EventManager::getRoleMedicine(int rnum, bool equip = false) {
	short medicine = Save::getInstance()->m_Character[rnum].AttPoison;
	if (Save::getInstance()->m_Character[rnum].GongTi > -1) {
		int magicnum = Save::getInstance()->m_Character[rnum].LMagic[Save::getInstance()->m_Character[rnum].GongTi];
		if (getGongtiLevel(rnum, Save::getInstance()->m_Character[rnum].GongTi) == Save::getInstance()->m_Magic[magicnum].MaxLevel) {
			medicine += Save::getInstance()->m_Magic[magicnum].AddMedcine;
		}
	}
	if (equip) {
		for (int i = 0; i < config::MaxEquipNum; i++) {
			if (Save::getInstance()->m_Character[rnum].Equip[i] >= 0) {
				medicine += Save::getInstance()->m_Item[Save::getInstance()->m_Character[rnum].Equip[i]].AddMedcine;
			}
		}
	}
	return medicine;
}

void EventManager::JumpScene(int snum, int x =-2, int y=-2) {
	int curScene = snum;
	if (x == -2) {
		x = Save::getInstance()->m_SceneData[curScene].EntranceX;
	}
	if (y == -2) {
		y = Save::getInstance()->m_SceneData[curScene].EntranceY;
	}
	Save::getInstance()->m_Character[0].CurrentPosition = curScene;
	auto s = new SubScene(curScene);
	s->setPosition(x, y);
	s->push(s);
	if (Save::getInstance()->m_SceneData[curScene].IsBattle&&Save::getInstance()->m_Character[0].Faction == Save::getInstance()->m_SceneData[curScene].Faction) {
		//Faction battle need to be finished.
	}
}

int EventManager::GetItemCount(int inum) {

	for (int i = 0; i < config::MAX_ITEM_AMOUNT - 1; i++) {
		if (Save::getInstance()->m_BasicData[0].m_RItemList->Number == inum) {
			return Save::getInstance()->m_BasicData[0].m_RItemList->Amount;
		}
	}
}

void EventManager::StudyMagic(int rnum, int magicnum, int newmagicnum, int level, int dismode) {
	int max0,tmp;
	if (dismode == 1) {
		//nend new talk
	}
	else if (newmagicnum == 0) {		//delete magic
		for (int i = 0; i < config::MaxMagicNum - 1; i++) {
			if (Save::getInstance()->m_Character[rnum].LMagic[i] == magicnum) {				
				for (int n = i; n < config::MaxMagicNum - 2; n++) {
					Save::getInstance()->m_Character[rnum].LMagic[n] = Save::getInstance()->m_Character[rnum].LMagic[n + 1];
					Save::getInstance()->m_Character[rnum].MagLevel[n] = Save::getInstance()->m_Character[rnum].MagLevel[n + 1];
				}
				Save::getInstance()->m_Character[rnum].LMagic[29] = 0;
				Save::getInstance()->m_Character[rnum].MagLevel[29] = 0;
				break;
			}
		}
		
	}
	else {
		for (int i = 0; i < config::MaxMagicNum - 1; i++) {
			if (Save::getInstance()->m_Character[rnum].LMagic[i] == newmagicnum) {
				if (level == -2) {
					level = 0;
				}
				max0 = 9;
				if (Save::getInstance()->m_Magic[newmagicnum].MagicType == 5) {
					max0 = Save::getInstance()->m_Magic[newmagicnum].MaxLevel;
				}
				tmp = Save::getInstance()->m_Character[rnum].MagLevel[i];
				if (max0 * 100 + 99 > Save::getInstance()->m_Character[rnum].MagLevel[i] + level + 100) {
					Save::getInstance()->m_Character[rnum].MagLevel[i] = Save::getInstance()->m_Character[rnum].MagLevel[i] + level + 100;
				}
				else {
					Save::getInstance()->m_Character[rnum].MagLevel[i] = max0 * 100 + 99;
				}
				StudyMagic(rnum, magicnum, 0, 0, 0);
				break;
			}
		}
		if (magicnum > 0) {
		for (int i = 0; i < config::MaxMagicNum - 1; i++) {
			if (Save::getInstance()->m_Character[rnum].LMagic[i] == magicnum || Save::getInstance()->m_Character[rnum].LMagic[i] < 0) {
				if (level != -2) {
					Save::getInstance()->m_Character[rnum].MagLevel[i] = level;
				}
				Save::getInstance()->m_Character[rnum].LMagic[i] == newmagicnum;
				break;
			}
		}			
		}
	}
}


//#undef EVENT_FUNC

