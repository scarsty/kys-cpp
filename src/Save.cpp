#include "Save.h"
#include "File.h"
#include "GrpIdxFile.h"
#include "PotConv.h"
#include "csv.h"
#include "libconvert.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

Save::Save()
{
}

Save::~Save()
{
}

std::string Save::getFilename(int i, char c)
{
    std::string filename;
    if (i > 0)
    {
        filename = convert::formatString("../game/save/%c%d.grp", c, i);
        if (c == 'r')
        {
            filename += "32";
        }
    }
    else
    {
        if (c == 'r')
        {
            filename = "../game/save/ranger.grp32";
        }
        else if (c == 's')
        {
            filename = "../game/save/allsin.grp";
        }
        else if (c == 'd')
        {
            filename = "../game/save/alldef.grp";
        }
    }
    return filename;
}

bool Save::checkSaveFileExist(int num)
{
    return File::fileExist(getFilename(num, 'r'))
        && File::fileExist(getFilename(num, 's'))
        && File::fileExist(getFilename(num, 'd'));
}

void Save::updateAllPtrVector()
{
    toPtrVector(roles_mem_, roles_);
    toPtrVector(items_mem_, items_);
    toPtrVector(submap_infos_mem_, submap_infos_);
    toPtrVector(magics_mem_, magics_);
    toPtrVector(shops_mem_, shops_);
}

bool Save::load(int num)
{
    if (!checkSaveFileExist(num))
    {
        return false;
    }

    loadR(num);
    loadSD(num);

    //�ڲ�����Ϊcp936
    if (Encode != 936)
    {
        Encode = 936;
        for (auto i : roles_)
        {
            PotConv::fromCP950ToCP936(i->Name);
            PotConv::fromCP950ToCP936(i->Nick);
        }
        for (auto i : items_)
        {
            PotConv::fromCP950ToCP936(i->Name);
            PotConv::fromCP950ToCP936(i->Introduction);
        }
        for (auto i : magics_)
        {
            PotConv::fromCP950ToCP936(i->Name);
        }
        for (auto i : submap_infos_)
        {
            PotConv::fromCP950ToCP936(i->Name);
        }
    }

    makeMaps();

    return true;
}

void Save::loadR(int num)
{
    std::string filenamer = getFilename(num, 'r');
    std::string filename_idx = "../game/save/ranger.idx32";
    auto rgrp = GrpIdxFile::getIdxContent(filename_idx, filenamer, &offset_, &length_);

    memcpy(&InShip, rgrp + offset_[0], length_[0]);
    File::readDataToVector(rgrp + offset_[1], length_[1], roles_mem_, sizeof(RoleSave));
    File::readDataToVector(rgrp + offset_[2], length_[2], items_mem_, sizeof(ItemSave));
    File::readDataToVector(rgrp + offset_[3], length_[3], submap_infos_mem_, sizeof(SubMapInfoSave));
    File::readDataToVector(rgrp + offset_[4], length_[4], magics_mem_, sizeof(MagicSave));
    File::readDataToVector(rgrp + offset_[5], length_[5], shops_mem_, sizeof(ShopSave));

    updateAllPtrVector();

    delete[] rgrp;
}

void Save::loadSD(int num)
{
    std::string filenames = getFilename(num, 's');
    std::string filenamed = getFilename(num, 'd');

    auto submap_count = submap_infos_.size();

    auto sdata = new char[submap_count * sdata_length_];
    auto ddata = new char[submap_count * ddata_length_];
    File::readFile(filenames, sdata, submap_count * sdata_length_);
    File::readFile(filenamed, ddata, submap_count * ddata_length_);
    for (int i = 0; i < submap_count; i++)
    {
        memcpy(&(submap_infos_mem_[i].LayerData(0, 0, 0)), sdata + sdata_length_ * i, sdata_length_);
        memcpy(submap_infos_mem_[i].Event(0), ddata + ddata_length_ * i, ddata_length_);
    }
    delete[] sdata;
    delete[] ddata;

}

bool Save::save(int num)
{
    saveR(num);
    saveSD(num);
    return true;
}

void Save::saveR(int num)
{
    std::string filenamer = getFilename(num, 'r');

    char* rgrp = new char[offset_.back()];
    memcpy(rgrp + offset_[0], &InShip, length_[0]);
    File::writeVectorToData(rgrp + offset_[1], length_[1], roles_mem_, sizeof(RoleSave));
    File::writeVectorToData(rgrp + offset_[2], length_[2], items_mem_, sizeof(ItemSave));
    File::writeVectorToData(rgrp + offset_[3], length_[3], submap_infos_mem_, sizeof(SubMapInfoSave));
    File::writeVectorToData(rgrp + offset_[4], length_[4], magics_mem_, sizeof(MagicSave));
    File::writeVectorToData(rgrp + offset_[5], length_[5], shops_mem_, sizeof(ShopSave));

    File::writeFile(filenamer, rgrp, offset_.back());
    delete[] rgrp;
}

void Save::saveSD(int num)
{
    std::string filenames = getFilename(num, 's');
    std::string filenamed = getFilename(num, 'd');
    auto submap_count = submap_infos_mem_.size();

    auto sdata = new char[submap_count * sdata_length_];
    auto ddata = new char[submap_count * ddata_length_];
    for (int i = 0; i < submap_count; i++)
    {
        memcpy(sdata + sdata_length_ * i, &(submap_infos_mem_[i].LayerData(0, 0, 0)), sdata_length_);
        memcpy(ddata + ddata_length_ * i, submap_infos_mem_[i].Event(0), ddata_length_);
    }
    File::writeFile(filenames, sdata, submap_count * sdata_length_);
    File::writeFile(filenamed, ddata, submap_count * ddata_length_);
    delete[] sdata;
    delete[] ddata;
}

void Save::resetRData(const std::vector<RoleSave>& newData)
{
    roles_mem_.clear();
    for (int i = 0; i < newData.size(); i++) {
        roles_mem_.emplace_back();
        auto& r = roles_mem_.back();
        memcpy(&r, &newData[i], sizeof(RoleSave));
        r.RealID = r.ID;
        r.ID = i;
    }
    updateAllPtrVector();
}

Role* Save::getTeamMate(int i)
{
    if (i < 0 || i >= TEAMMATE_COUNT)
    {
        return nullptr;
    }
    int r = Team[i];
    if (r < 0 || r >= roles_mem_.size())
    {
        return nullptr;
    }
    return roles_[r];
}

Item* Save::getItemByBagIndex(int i)
{
    if (i < 0 || i >= ITEM_IN_BAG_COUNT)
    {
        return nullptr;
    }
    int r = Items[i].item_id;
    if (r < 0 || r >= items_mem_.size())
    {
        return nullptr;
    }
    return items_[r];
}

int Save::getItemCountByBagIndex(int i)
{
    return Items[i].count;
}

int Save::getItemCountInBag(Item* item)
{
    return getItemCountInBag(item->ID);
}

int Save::getItemCountInBag(int item_id)
{
    for (int i = 0; i < ITEM_IN_BAG_COUNT; i++)
    {
        auto id = Items[i].item_id;
        if (id < 0)
        {
            break;
        }
        if (id == item_id)
        {
            return Items[i].count;
        }
    }
    return 0;
}

int Save::getMoneyCountInBag()
{
    return getItemCountInBag(Item::MoneyItemID);
}

void Save::makeMaps()
{
    roles_by_name_.clear();
    magics_by_name_.clear();
    items_by_name_.clear();
    submap_infos_by_name_.clear();

    //�������ģ�����ʹ��
    for (auto& i : roles_)
    {
        roles_by_name_[i->Name] = i;
    }
    for (auto& i : magics_)
    {
        magics_by_name_[i->Name] = i;
    }
    for (auto& i : items_)
    {
        items_by_name_[i->Name] = i;
    }
    for (auto& i : submap_infos_)
    {
        submap_infos_by_name_[i->Name] = i;
    }
}

Magic* Save::getRoleLearnedMagic(Role* r, int i)
{
    if (i < 0 || i >= ROLE_MAGIC_COUNT)
    {
        return nullptr;
    }
    return getMagic(r->MagicID[i]);
}

int Save::getRoleLearnedMagicLevelIndex(Role* r, Magic* m)
{
    for (int i = 0; i < ROLE_MAGIC_COUNT; i++)
    {
        if (r->MagicID[i] == m->ID)
        {
            return r->getRoleMagicLevelIndex(i);
        }
    }
    return -1;
}

void Save::saveRToCSV(int num)
{
    NewSave::SaveToCSVBaseInfo((BaseInfo*)this, 1, num);
    // ����
    NewSave::SaveToCSVItemList(Items, ITEM_IN_BAG_COUNT, num);
    // ����
    NewSave::SaveToCSVRoleSave(roles_mem_, num);
    // ��Ʒ
    NewSave::SaveToCSVItemSave(items_mem_, num);
    // ����
    NewSave::SaveToCSVSubMapInfoSave(submap_infos_mem_, num);
    // �书
    NewSave::SaveToCSVMagicSave(magics_mem_, num);
    // �̵�
    NewSave::SaveToCSVShopSave(shops_mem_, num);
}

void Save::loadRFromCSV(int num)
{
    NewSave::LoadFromCSVBaseInfo((BaseInfo*)this, 1, num);
    NewSave::LoadFromCSVItemList(Items, ITEM_IN_BAG_COUNT, num);
    NewSave::LoadFromCSVRoleSave(roles_mem_, num);
    NewSave::LoadFromCSVItemSave(items_mem_, num);
    NewSave::LoadFromCSVSubMapInfoSave(submap_infos_mem_, num);
    NewSave::LoadFromCSVMagicSave(magics_mem_, num);
    NewSave::LoadFromCSVShopSave(shops_mem_, num);
    updateAllPtrVector();
    makeMaps();
}

bool Save::insertAt(const std::string& type, int idx)
{
    if (type == u8"Role")
    {
        NewSave::InsertRoleAt(roles_mem_, idx);
        return true;
    }
    else if (type == u8"Item")
    {
        NewSave::InsertItemAt(items_mem_, idx);
        return true;
    }
    else if (type == u8"Magic")
    {
        NewSave::InsertMagicAt(magics_mem_, idx);
        return true;
    }
    else if (type == u8"SubMapInfo")
    {
        NewSave::InsertSubMapInfoAt(submap_infos_mem_, idx);
        return true;
    }
    else if (type == u8"Shop")
    {
        NewSave::InsertShopAt(shops_mem_, idx);
        return true;
    }
    return false;
}

// ����
void Save::NewSave::SaveToCSVBaseInfo(BaseInfo* data, int length, int record)
{
    std::ofstream fout("../game/save/csv/" + std::to_string(record) + "_����.csv");
    fout << "�˴�";
    fout << ",";
    fout << "�ӳ�����";
    fout << ",";
    fout << "����ͼX";
    fout << ",";
    fout << "����ͼY";
    fout << ",";
    fout << "�ӳ���X";
    fout << ",";
    fout << "�ӳ���Y";
    fout << ",";
    fout << "�泯����";
    fout << ",";
    fout << "��X";
    fout << ",";
    fout << "��Y";
    fout << ",";
    fout << "��X1";
    fout << ",";
    fout << "��Y1";
    fout << ",";
    fout << "�ڲ�����";
    fout << ",";
    fout << "����1";
    fout << ",";
    fout << "����2";
    fout << ",";
    fout << "����3";
    fout << ",";
    fout << "����4";
    fout << ",";
    fout << "����5";
    fout << ",";
    fout << "����6";
    fout << std::endl;
    for (int i = 0; i < length; i++)
    {
        fout << data[i].InShip;
        fout << ",";
        fout << data[i].InSubMap;
        fout << ",";
        fout << data[i].MainMapX;
        fout << ",";
        fout << data[i].MainMapY;
        fout << ",";
        fout << data[i].SubMapX;
        fout << ",";
        fout << data[i].SubMapY;
        fout << ",";
        fout << data[i].FaceTowards;
        fout << ",";
        fout << data[i].ShipX;
        fout << ",";
        fout << data[i].ShipY;
        fout << ",";
        fout << data[i].ShipX1;
        fout << ",";
        fout << data[i].ShipY1;
        fout << ",";
        fout << data[i].Encode;
        fout << ",";
        for (int j = 0; j < 6; j++)
        {
            fout << data[i].Team[j];
            if (j != 6 - 1)
            { fout << ","; }
        }
        fout << std::endl;
    }
}
// ����
void Save::NewSave::SaveToCSVItemList(ItemList* data, int length, int record)
{
    std::ofstream fout("../game/save/csv/" + std::to_string(record) + "_����.csv");
    fout << "��Ʒ���";
    fout << ",";
    fout << "��Ʒ����";
    fout << std::endl;
    for (int i = 0; i < length; i++)
    {
        fout << data[i].item_id;
        fout << ",";
        fout << data[i].count;
        fout << std::endl;
    }
}
// ����
void Save::NewSave::SaveToCSVRoleSave(const std::vector<Role>& data, int record)
{
    std::ofstream fout("../game/save/csv/" + std::to_string(record) + "_����.csv");
    fout << "���";
    fout << ",";
    fout << "ͷ��";
    fout << ",";
    fout << "�����ɳ�";
    fout << ",";
    fout << "����";
    fout << ",";
    fout << "����";
    fout << ",";
    fout << "���";
    fout << ",";
    fout << "�Ա�";
    fout << ",";
    fout << "�ȼ�";
    fout << ",";
    fout << "����";
    fout << ",";
    fout << "����";
    fout << ",";
    fout << "�������ֵ";
    fout << ",";
    fout << "����";
    fout << ",";
    fout << "�ж�";
    fout << ",";
    fout << "����";
    fout << ",";
    fout << "��Ʒ��������";
    fout << ",";
    fout << "����";
    fout << ",";
    fout << "����";
    fout << ",";
    fout << "����֡��1";
    fout << ",";
    fout << "����֡��2";
    fout << ",";
    fout << "����֡��3";
    fout << ",";
    fout << "����֡��4";
    fout << ",";
    fout << "����֡��5";
    fout << ",";
    fout << "����֡��6";
    fout << ",";
    fout << "����֡��7";
    fout << ",";
    fout << "����֡��8";
    fout << ",";
    fout << "����֡��9";
    fout << ",";
    fout << "����֡��10";
    fout << ",";
    fout << "����֡��11";
    fout << ",";
    fout << "����֡��12";
    fout << ",";
    fout << "����֡��13";
    fout << ",";
    fout << "����֡��14";
    fout << ",";
    fout << "����֡��15";
    fout << ",";
    fout << "��������";
    fout << ",";
    fout << "����";
    fout << ",";
    fout << "�������ֵ";
    fout << ",";
    fout << "������";
    fout << ",";
    fout << "�Ṧ";
    fout << ",";
    fout << "������";
    fout << ",";
    fout << "ҽ��";
    fout << ",";
    fout << "�ö�";
    fout << ",";
    fout << "�ⶾ";
    fout << ",";
    fout << "����";
    fout << ",";
    fout << "ȭ��";
    fout << ",";
    fout << "����";
    fout << ",";
    fout << "ˣ��";
    fout << ",";
    fout << "����";
    fout << ",";
    fout << "����";
    fout << ",";
    fout << "��ѧ��ʶ";
    fout << ",";
    fout << "Ʒ��";
    fout << ",";
    fout << "��������";
    fout << ",";
    fout << "���һ���";
    fout << ",";
    fout << "����";
    fout << ",";
    fout << "����";
    fout << ",";
    fout << "������Ʒ";
    fout << ",";
    fout << "��������";
    fout << ",";
    fout << "�����书1";
    fout << ",";
    fout << "�����书2";
    fout << ",";
    fout << "�����书3";
    fout << ",";
    fout << "�����书4";
    fout << ",";
    fout << "�����书5";
    fout << ",";
    fout << "�����书6";
    fout << ",";
    fout << "�����书7";
    fout << ",";
    fout << "�����书8";
    fout << ",";
    fout << "�����书9";
    fout << ",";
    fout << "�����书10";
    fout << ",";
    fout << "�书�ȼ�1";
    fout << ",";
    fout << "�书�ȼ�2";
    fout << ",";
    fout << "�书�ȼ�3";
    fout << ",";
    fout << "�书�ȼ�4";
    fout << ",";
    fout << "�书�ȼ�5";
    fout << ",";
    fout << "�书�ȼ�6";
    fout << ",";
    fout << "�书�ȼ�7";
    fout << ",";
    fout << "�书�ȼ�8";
    fout << ",";
    fout << "�书�ȼ�9";
    fout << ",";
    fout << "�书�ȼ�10";
    fout << ",";
    fout << "Я����Ʒ1";
    fout << ",";
    fout << "Я����Ʒ2";
    fout << ",";
    fout << "Я����Ʒ3";
    fout << ",";
    fout << "Я����Ʒ4";
    fout << ",";
    fout << "Я����Ʒ����1";
    fout << ",";
    fout << "Я����Ʒ����2";
    fout << ",";
    fout << "Я����Ʒ����3";
    fout << ",";
    fout << "Я����Ʒ����4";
    fout << std::endl;
    int length = data.size();
    for (int i = 0; i < length; i++)
    {
        fout << data[i].ID;
        fout << ",";
        fout << data[i].HeadID;
        fout << ",";
        fout << data[i].IncLife;
        fout << ",";
        fout << data[i].UnUse;
        fout << ",";
        fout << '"' << data[i].Name << '"';
        fout << ",";
        fout << '"' << data[i].Nick << '"';
        fout << ",";
        fout << data[i].Sexual;
        fout << ",";
        fout << data[i].Level;
        fout << ",";
        fout << data[i].Exp;
        fout << ",";
        fout << data[i].HP;
        fout << ",";
        fout << data[i].MaxHP;
        fout << ",";
        fout << data[i].Hurt;
        fout << ",";
        fout << data[i].Poison;
        fout << ",";
        fout << data[i].PhysicalPower;
        fout << ",";
        fout << data[i].ExpForMakeItem;
        fout << ",";
        fout << data[i].Equip0;
        fout << ",";
        fout << data[i].Equip1;
        fout << ",";
        for (int j = 0; j < 15; j++)
        {
            fout << data[i].Frame[j];
            if (j != 15 - 1)
            { fout << ","; }
        }
        fout << ",";
        fout << data[i].MPType;
        fout << ",";
        fout << data[i].MP;
        fout << ",";
        fout << data[i].MaxMP;
        fout << ",";
        fout << data[i].Attack;
        fout << ",";
        fout << data[i].Speed;
        fout << ",";
        fout << data[i].Defence;
        fout << ",";
        fout << data[i].Medicine;
        fout << ",";
        fout << data[i].UsePoison;
        fout << ",";
        fout << data[i].Detoxification;
        fout << ",";
        fout << data[i].AntiPoison;
        fout << ",";
        fout << data[i].Fist;
        fout << ",";
        fout << data[i].Sword;
        fout << ",";
        fout << data[i].Knife;
        fout << ",";
        fout << data[i].Unusual;
        fout << ",";
        fout << data[i].HiddenWeapon;
        fout << ",";
        fout << data[i].Knowledge;
        fout << ",";
        fout << data[i].Morality;
        fout << ",";
        fout << data[i].AttackWithPoison;
        fout << ",";
        fout << data[i].AttackTwice;
        fout << ",";
        fout << data[i].Fame;
        fout << ",";
        fout << data[i].IQ;
        fout << ",";
        fout << data[i].PracticeItem;
        fout << ",";
        fout << data[i].ExpForItem;
        fout << ",";
        for (int j = 0; j < 10; j++)
        {
            fout << data[i].MagicID[j];
            if (j != 10 - 1)
            { fout << ","; }
        }
        fout << ",";
        for (int j = 0; j < 10; j++)
        {
            fout << data[i].MagicLevel[j];
            if (j != 10 - 1)
            { fout << ","; }
        }
        fout << ",";
        for (int j = 0; j < 4; j++)
        {
            fout << data[i].TakingItem[j];
            if (j != 4 - 1)
            { fout << ","; }
        }
        fout << ",";
        for (int j = 0; j < 4; j++)
        {
            fout << data[i].TakingItemCount[j];
            if (j != 4 - 1)
            { fout << ","; }
        }
        fout << std::endl;
    }
}
// ��Ʒ
void Save::NewSave::SaveToCSVItemSave(const std::vector<Item>& data, int record)
{
    std::ofstream fout("../game/save/csv/" + std::to_string(record) + "_��Ʒ.csv");
    fout << "���";
    fout << ",";
    fout << "��Ʒ��";
    fout << ",";
    fout << "��Ʒ������1";
    fout << ",";
    fout << "��Ʒ������2";
    fout << ",";
    fout << "��Ʒ������3";
    fout << ",";
    fout << "��Ʒ������4";
    fout << ",";
    fout << "��Ʒ������5";
    fout << ",";
    fout << "��Ʒ������6";
    fout << ",";
    fout << "��Ʒ������7";
    fout << ",";
    fout << "��Ʒ������8";
    fout << ",";
    fout << "��Ʒ������9";
    fout << ",";
    fout << "��Ʒ������10";
    fout << ",";
    fout << "��Ʒ˵��";
    fout << ",";
    fout << "�����书";
    fout << ",";
    fout << "�����������";
    fout << ",";
    fout << "ʹ����";
    fout << ",";
    fout << "װ������";
    fout << ",";
    fout << "��ʾ��Ʒ˵��";
    fout << ",";
    fout << "��Ʒ����";
    fout << ",";
    fout << "δ֪5";
    fout << ",";
    fout << "δ֪6";
    fout << ",";
    fout << "δ֪7";
    fout << ",";
    fout << "������";
    fout << ",";
    fout << "���������ֵ";
    fout << ",";
    fout << "���ж��ⶾ";
    fout << ",";
    fout << "������";
    fout << ",";
    fout << "�ı���������";
    fout << ",";
    fout << "������";
    fout << ",";
    fout << "���������ֵ";
    fout << ",";
    fout << "�ӹ�����";
    fout << ",";
    fout << "���Ṧ";
    fout << ",";
    fout << "�ӷ�����";
    fout << ",";
    fout << "��ҽ��";
    fout << ",";
    fout << "��ʹ��";
    fout << ",";
    fout << "�ӽⶾ";
    fout << ",";
    fout << "�ӿ���";
    fout << ",";
    fout << "��ȭ��";
    fout << ",";
    fout << "������";
    fout << ",";
    fout << "��ˣ��";
    fout << ",";
    fout << "���������";
    fout << ",";
    fout << "�Ӱ�������";
    fout << ",";
    fout << "����ѧ��ʶ";
    fout << ",";
    fout << "��Ʒ��";
    fout << ",";
    fout << "�����һ���";
    fout << ",";
    fout << "�ӹ�������";
    fout << ",";
    fout << "����������";
    fout << ",";
    fout << "����������";
    fout << ",";
    fout << "������";
    fout << ",";
    fout << "�蹥����";
    fout << ",";
    fout << "���Ṧ";
    fout << ",";
    fout << "���ö�";
    fout << ",";
    fout << "��ҽ��";
    fout << ",";
    fout << "��ⶾ";
    fout << ",";
    fout << "��ȭ��";
    fout << ",";
    fout << "������";
    fout << ",";
    fout << "��ˣ��";
    fout << ",";
    fout << "���������";
    fout << ",";
    fout << "�谵��";
    fout << ",";
    fout << "������";
    fout << ",";
    fout << "�辭��";
    fout << ",";
    fout << "������Ʒ�辭��";
    fout << ",";
    fout << "�����";
    fout << ",";
    fout << "������Ʒ1";
    fout << ",";
    fout << "������Ʒ2";
    fout << ",";
    fout << "������Ʒ3";
    fout << ",";
    fout << "������Ʒ4";
    fout << ",";
    fout << "������Ʒ5";
    fout << ",";
    fout << "������Ʒ����1";
    fout << ",";
    fout << "������Ʒ����2";
    fout << ",";
    fout << "������Ʒ����3";
    fout << ",";
    fout << "������Ʒ����4";
    fout << ",";
    fout << "������Ʒ����5";
    fout << std::endl;
    int length = data.size();
    for (int i = 0; i < length; i++)
    {
        fout << data[i].ID;
        fout << ",";
        fout << '"' << data[i].Name << '"';
        fout << ",";
        for (int j = 0; j < 10; j++)
        {
            fout << data[i].Name1[j];
            if (j != 10 - 1)
            { fout << ","; }
        }
        fout << ",";
        fout << '"' << data[i].Introduction << '"';
        fout << ",";
        fout << data[i].MagicID;
        fout << ",";
        fout << data[i].HiddenWeaponEffectID;
        fout << ",";
        fout << data[i].User;
        fout << ",";
        fout << data[i].EquipType;
        fout << ",";
        fout << data[i].ShowIntroduction;
        fout << ",";
        fout << data[i].ItemType;
        fout << ",";
        fout << data[i].UnKnown5;
        fout << ",";
        fout << data[i].UnKnown6;
        fout << ",";
        fout << data[i].UnKnown7;
        fout << ",";
        fout << data[i].AddHP;
        fout << ",";
        fout << data[i].AddMaxHP;
        fout << ",";
        fout << data[i].AddPoison;
        fout << ",";
        fout << data[i].AddPhysicalPower;
        fout << ",";
        fout << data[i].ChangeMPType;
        fout << ",";
        fout << data[i].AddMP;
        fout << ",";
        fout << data[i].AddMaxMP;
        fout << ",";
        fout << data[i].AddAttack;
        fout << ",";
        fout << data[i].AddSpeed;
        fout << ",";
        fout << data[i].AddDefence;
        fout << ",";
        fout << data[i].AddMedicine;
        fout << ",";
        fout << data[i].AddUsePoison;
        fout << ",";
        fout << data[i].AddDetoxification;
        fout << ",";
        fout << data[i].AddAntiPoison;
        fout << ",";
        fout << data[i].AddFist;
        fout << ",";
        fout << data[i].AddSword;
        fout << ",";
        fout << data[i].AddKnife;
        fout << ",";
        fout << data[i].AddUnusual;
        fout << ",";
        fout << data[i].AddHiddenWeapon;
        fout << ",";
        fout << data[i].AddKnowledge;
        fout << ",";
        fout << data[i].AddMorality;
        fout << ",";
        fout << data[i].AddAttackTwice;
        fout << ",";
        fout << data[i].AddAttackWithPoison;
        fout << ",";
        fout << data[i].OnlySuitableRole;
        fout << ",";
        fout << data[i].NeedMPType;
        fout << ",";
        fout << data[i].NeedMP;
        fout << ",";
        fout << data[i].NeedAttack;
        fout << ",";
        fout << data[i].NeedSpeed;
        fout << ",";
        fout << data[i].NeedUsePoison;
        fout << ",";
        fout << data[i].NeedMedicine;
        fout << ",";
        fout << data[i].NeedDetoxification;
        fout << ",";
        fout << data[i].NeedFist;
        fout << ",";
        fout << data[i].NeedSword;
        fout << ",";
        fout << data[i].NeedKnife;
        fout << ",";
        fout << data[i].NeedUnusual;
        fout << ",";
        fout << data[i].NeedHiddenWeapon;
        fout << ",";
        fout << data[i].NeedIQ;
        fout << ",";
        fout << data[i].NeedExp;
        fout << ",";
        fout << data[i].NeedExpForMakeItem;
        fout << ",";
        fout << data[i].NeedMaterial;
        fout << ",";
        for (int j = 0; j < 5; j++)
        {
            fout << data[i].MakeItem[j];
            if (j != 5 - 1)
            { fout << ","; }
        }
        fout << ",";
        for (int j = 0; j < 5; j++)
        {
            fout << data[i].MakeItemCount[j];
            if (j != 5 - 1)
            { fout << ","; }
        }
        fout << std::endl;
    }
}
// ����
void Save::NewSave::SaveToCSVSubMapInfoSave(const std::vector<SubMapInfo>& data, int record)
{
    std::ofstream fout("../game/save/csv/" + std::to_string(record) + "_����.csv");
    fout << "���";
    fout << ",";
    fout << "����";
    fout << ",";
    fout << "��������";
    fout << ",";
    fout << "��������";
    fout << ",";
    fout << "��ת����";
    fout << ",";
    fout << "��������";
    fout << ",";
    fout << "�⾰���X1";
    fout << ",";
    fout << "�⾰���Y1";
    fout << ",";
    fout << "�⾰���X2";
    fout << ",";
    fout << "�⾰���Y2";
    fout << ",";
    fout << "���X";
    fout << ",";
    fout << "���Y";
    fout << ",";
    fout << "����X1";
    fout << ",";
    fout << "����X2";
    fout << ",";
    fout << "����X3";
    fout << ",";
    fout << "����Y1";
    fout << ",";
    fout << "����Y2";
    fout << ",";
    fout << "����Y3";
    fout << ",";
    fout << "��תX";
    fout << ",";
    fout << "��תY";
    fout << ",";
    fout << "��ת����X";
    fout << ",";
    fout << "��ת����Y";
    fout << std::endl;
    int length = data.size();
    for (int i = 0; i < length; i++)
    {
        fout << data[i].ID;
        fout << ",";
        fout << '"' << data[i].Name << '"';
        fout << ",";
        fout << data[i].ExitMusic;
        fout << ",";
        fout << data[i].EntranceMusic;
        fout << ",";
        fout << data[i].JumpSubMap;
        fout << ",";
        fout << data[i].EntranceCondition;
        fout << ",";
        fout << data[i].MainEntranceX1;
        fout << ",";
        fout << data[i].MainEntranceY1;
        fout << ",";
        fout << data[i].MainEntranceX2;
        fout << ",";
        fout << data[i].MainEntranceY2;
        fout << ",";
        fout << data[i].EntranceX;
        fout << ",";
        fout << data[i].EntranceY;
        fout << ",";
        for (int j = 0; j < 3; j++)
        {
            fout << data[i].ExitX[j];
            if (j != 3 - 1)
            { fout << ","; }
        }
        fout << ",";
        for (int j = 0; j < 3; j++)
        {
            fout << data[i].ExitY[j];
            if (j != 3 - 1)
            { fout << ","; }
        }
        fout << ",";
        fout << data[i].JumpX;
        fout << ",";
        fout << data[i].JumpY;
        fout << ",";
        fout << data[i].JumpReturnX;
        fout << ",";
        fout << data[i].JumpReturnY;
        fout << std::endl;
    }
}
// �书
void Save::NewSave::SaveToCSVMagicSave(const std::vector<Magic>& data, int record)
{
    std::ofstream fout("../game/save/csv/" + std::to_string(record) + "_�书.csv");
    fout << "���";
    fout << ",";
    fout << "����";
    fout << ",";
    fout << "δ֪1";
    fout << ",";
    fout << "δ֪2";
    fout << ",";
    fout << "δ֪3";
    fout << ",";
    fout << "δ֪4";
    fout << ",";
    fout << "δ֪5";
    fout << ",";
    fout << "������Ч";
    fout << ",";
    fout << "�书����";
    fout << ",";
    fout << "�书����";
    fout << ",";
    fout << "�˺�����";
    fout << ",";
    fout << "������Χ����";
    fout << ",";
    fout << "��������";
    fout << ",";
    fout << "�����ж�";
    fout << ",";
    fout << "����1";
    fout << ",";
    fout << "����2";
    fout << ",";
    fout << "����3";
    fout << ",";
    fout << "����4";
    fout << ",";
    fout << "����5";
    fout << ",";
    fout << "����6";
    fout << ",";
    fout << "����7";
    fout << ",";
    fout << "����8";
    fout << ",";
    fout << "����9";
    fout << ",";
    fout << "����10";
    fout << ",";
    fout << "�ƶ���Χ1";
    fout << ",";
    fout << "�ƶ���Χ2";
    fout << ",";
    fout << "�ƶ���Χ3";
    fout << ",";
    fout << "�ƶ���Χ4";
    fout << ",";
    fout << "�ƶ���Χ5";
    fout << ",";
    fout << "�ƶ���Χ6";
    fout << ",";
    fout << "�ƶ���Χ7";
    fout << ",";
    fout << "�ƶ���Χ8";
    fout << ",";
    fout << "�ƶ���Χ9";
    fout << ",";
    fout << "�ƶ���Χ10";
    fout << ",";
    fout << "ɱ�˷�Χ1";
    fout << ",";
    fout << "ɱ�˷�Χ2";
    fout << ",";
    fout << "ɱ�˷�Χ3";
    fout << ",";
    fout << "ɱ�˷�Χ4";
    fout << ",";
    fout << "ɱ�˷�Χ5";
    fout << ",";
    fout << "ɱ�˷�Χ6";
    fout << ",";
    fout << "ɱ�˷�Χ7";
    fout << ",";
    fout << "ɱ�˷�Χ8";
    fout << ",";
    fout << "ɱ�˷�Χ9";
    fout << ",";
    fout << "ɱ�˷�Χ10";
    fout << ",";
    fout << "������1";
    fout << ",";
    fout << "������2";
    fout << ",";
    fout << "������3";
    fout << ",";
    fout << "������4";
    fout << ",";
    fout << "������5";
    fout << ",";
    fout << "������6";
    fout << ",";
    fout << "������7";
    fout << ",";
    fout << "������8";
    fout << ",";
    fout << "������9";
    fout << ",";
    fout << "������10";
    fout << ",";
    fout << "ɱ������1";
    fout << ",";
    fout << "ɱ������2";
    fout << ",";
    fout << "ɱ������3";
    fout << ",";
    fout << "ɱ������4";
    fout << ",";
    fout << "ɱ������5";
    fout << ",";
    fout << "ɱ������6";
    fout << ",";
    fout << "ɱ������7";
    fout << ",";
    fout << "ɱ������8";
    fout << ",";
    fout << "ɱ������9";
    fout << ",";
    fout << "ɱ������10";
    fout << std::endl;
    int length = data.size();
    for (int i = 0; i < length; i++)
    {
        fout << data[i].ID;
        fout << ",";
        fout << '"' << data[i].Name << '"';
        fout << ",";
        for (int j = 0; j < 5; j++)
        {
            fout << data[i].Unknown[j];
            if (j != 5 - 1)
            { fout << ","; }
        }
        fout << ",";
        fout << data[i].SoundID;
        fout << ",";
        fout << data[i].MagicType;
        fout << ",";
        fout << data[i].EffectID;
        fout << ",";
        fout << data[i].HurtType;
        fout << ",";
        fout << data[i].AttackAreaType;
        fout << ",";
        fout << data[i].NeedMP;
        fout << ",";
        fout << data[i].WithPoison;
        fout << ",";
        for (int j = 0; j < 10; j++)
        {
            fout << data[i].Attack[j];
            if (j != 10 - 1)
            { fout << ","; }
        }
        fout << ",";
        for (int j = 0; j < 10; j++)
        {
            fout << data[i].SelectDistance[j];
            if (j != 10 - 1)
            { fout << ","; }
        }
        fout << ",";
        for (int j = 0; j < 10; j++)
        {
            fout << data[i].AttackDistance[j];
            if (j != 10 - 1)
            { fout << ","; }
        }
        fout << ",";
        for (int j = 0; j < 10; j++)
        {
            fout << data[i].AddMP[j];
            if (j != 10 - 1)
            { fout << ","; }
        }
        fout << ",";
        for (int j = 0; j < 10; j++)
        {
            fout << data[i].HurtMP[j];
            if (j != 10 - 1)
            { fout << ","; }
        }
        fout << std::endl;
    }
}
// �̵�
void Save::NewSave::SaveToCSVShopSave(const std::vector<Shop>& data, int record)
{
    std::ofstream fout("../game/save/csv/" + std::to_string(record) + "_�̵�.csv");
    fout << "��Ʒ���1";
    fout << ",";
    fout << "��Ʒ���2";
    fout << ",";
    fout << "��Ʒ���3";
    fout << ",";
    fout << "��Ʒ���4";
    fout << ",";
    fout << "��Ʒ���5";
    fout << ",";
    fout << "��Ʒ����1";
    fout << ",";
    fout << "��Ʒ����2";
    fout << ",";
    fout << "��Ʒ����3";
    fout << ",";
    fout << "��Ʒ����4";
    fout << ",";
    fout << "��Ʒ����5";
    fout << ",";
    fout << "��Ʒ�۸�1";
    fout << ",";
    fout << "��Ʒ�۸�2";
    fout << ",";
    fout << "��Ʒ�۸�3";
    fout << ",";
    fout << "��Ʒ�۸�4";
    fout << ",";
    fout << "��Ʒ�۸�5";
    fout << std::endl;
    int length = data.size();
    for (int i = 0; i < length; i++)
    {
        for (int j = 0; j < 5; j++)
        {
            fout << data[i].ItemID[j];
            if (j != 5 - 1)
            { fout << ","; }
        }
        fout << ",";
        for (int j = 0; j < 5; j++)
        {
            fout << data[i].Total[j];
            if (j != 5 - 1)
            { fout << ","; }
        }
        fout << ",";
        for (int j = 0; j < 5; j++)
        {
            fout << data[i].Price[j];
            if (j != 5 - 1)
            { fout << ","; }
        }
        fout << std::endl;
    }
}
// ����
void Save::NewSave::LoadFromCSVBaseInfo(BaseInfo* data, int length, int record)
{
    io::CSVReader<18, io::trim_chars<>, io::double_quote_escape<',', '\"'>> in("../game/save/csv/" + std::to_string(record) + "_����.csv");
    in.read_header(io::ignore_missing_column | io::ignore_extra_column,
        "�˴�",
        "�ӳ�����",
        "����ͼX",
        "����ͼY",
        "�ӳ���X",
        "�ӳ���Y",
        "�泯����",
        "��X",
        "��Y",
        "��X1",
        "��Y1",
        "�ڲ�����",
        "����1",
        "����2",
        "����3",
        "����4",
        "����5",
        "����6");
    auto getDefault = []()
    {
        BaseInfo nextLineData;
        nextLineData.InShip = 0;
        nextLineData.InSubMap = 0;
        nextLineData.MainMapX = 0;
        nextLineData.MainMapY = 0;
        nextLineData.SubMapX = 0;
        nextLineData.SubMapY = 0;
        nextLineData.FaceTowards = 0;
        nextLineData.ShipX = 0;
        nextLineData.ShipY = 0;
        nextLineData.ShipX1 = 0;
        nextLineData.ShipY1 = 0;
        nextLineData.Encode = 0;
        for (int j = 0; j < 6; j++)
        {
            nextLineData.Team[j] = -1;
        }
        return nextLineData;
    };
    int lines = 0;
    auto nextLineData = getDefault();
    while (in.read_row(
        nextLineData.InShip,
        nextLineData.InSubMap,
        nextLineData.MainMapX,
        nextLineData.MainMapY,
        nextLineData.SubMapX,
        nextLineData.SubMapY,
        nextLineData.FaceTowards,
        nextLineData.ShipX,
        nextLineData.ShipY,
        nextLineData.ShipX1,
        nextLineData.ShipY1,
        nextLineData.Encode,
        nextLineData.Team[0],
        nextLineData.Team[1],
        nextLineData.Team[2],
        nextLineData.Team[3],
        nextLineData.Team[4],
        nextLineData.Team[5]))
    {
        data[lines] = nextLineData;
        if (lines + 1 == length)
        { break; }
        lines++;
        nextLineData = getDefault();
    }
}
// ����
void Save::NewSave::LoadFromCSVItemList(ItemList* data, int length, int record)
{
    io::CSVReader<2, io::trim_chars<>, io::double_quote_escape<',', '\"'>> in("../game/save/csv/" + std::to_string(record) + "_����.csv");
    in.read_header(io::ignore_missing_column | io::ignore_extra_column,
        "��Ʒ���",
        "��Ʒ����");
    auto getDefault = []()
    {
        ItemList nextLineData;
        nextLineData.item_id = -1;
        nextLineData.count = 0;
        return nextLineData;
    };
    int lines = 0;
    auto nextLineData = getDefault();
    while (in.read_row(
        nextLineData.item_id,
        nextLineData.count))
    {
        data[lines] = nextLineData;
        if (lines + 1 == length)
        { break; }
        lines++;
        nextLineData = getDefault();
    }
}
// ����
void Save::NewSave::LoadFromCSVRoleSave(std::vector<Role>& data, int record)
{
    data.clear();
    io::CSVReader<83, io::trim_chars<>, io::double_quote_escape<',', '\"'>> in("../game/save/csv/" + std::to_string(record) + "_����.csv");
    in.read_header(io::ignore_missing_column | io::ignore_extra_column,
        "���",
        "ͷ��",
        "�����ɳ�",
        "����",
        "����",
        "���",
        "�Ա�",
        "�ȼ�",
        "����",
        "����",
        "�������ֵ",
        "����",
        "�ж�",
        "����",
        "��Ʒ��������",
        "����",
        "����",
        "����֡��1",
        "����֡��2",
        "����֡��3",
        "����֡��4",
        "����֡��5",
        "����֡��6",
        "����֡��7",
        "����֡��8",
        "����֡��9",
        "����֡��10",
        "����֡��11",
        "����֡��12",
        "����֡��13",
        "����֡��14",
        "����֡��15",
        "��������",
        "����",
        "�������ֵ",
        "������",
        "�Ṧ",
        "������",
        "ҽ��",
        "�ö�",
        "�ⶾ",
        "����",
        "ȭ��",
        "����",
        "ˣ��",
        "����",
        "����",
        "��ѧ��ʶ",
        "Ʒ��",
        "��������",
        "���һ���",
        "����",
        "����",
        "������Ʒ",
        "��������",
        "�����书1",
        "�����书2",
        "�����书3",
        "�����书4",
        "�����书5",
        "�����书6",
        "�����书7",
        "�����书8",
        "�����书9",
        "�����书10",
        "�书�ȼ�1",
        "�书�ȼ�2",
        "�书�ȼ�3",
        "�书�ȼ�4",
        "�书�ȼ�5",
        "�书�ȼ�6",
        "�书�ȼ�7",
        "�书�ȼ�8",
        "�书�ȼ�9",
        "�书�ȼ�10",
        "Я����Ʒ1",
        "Я����Ʒ2",
        "Я����Ʒ3",
        "Я����Ʒ4",
        "Я����Ʒ����1",
        "Я����Ʒ����2",
        "Я����Ʒ����3",
        "Я����Ʒ����4");
    auto getDefault = []()
    {
        Role nextLineData;
        nextLineData.ID = 0;
        nextLineData.HeadID = 0;
        nextLineData.IncLife = 0;
        nextLineData.UnUse = 0;
        memset(nextLineData.Name, '\0', sizeof(nextLineData.Name));
        memset(nextLineData.Nick, '\0', sizeof(nextLineData.Nick));
        nextLineData.Sexual = 0;
        nextLineData.Level = 0;
        nextLineData.Exp = 0;
        nextLineData.HP = 0;
        nextLineData.MaxHP = 0;
        nextLineData.Hurt = 0;
        nextLineData.Poison = 0;
        nextLineData.PhysicalPower = 0;
        nextLineData.ExpForMakeItem = 0;
        nextLineData.Equip0 = -1;
        nextLineData.Equip1 = -1;
        for (int j = 0; j < 15; j++)
        {
            nextLineData.Frame[j] = 0;
        }
        nextLineData.MPType = 0;
        nextLineData.MP = 0;
        nextLineData.MaxMP = 0;
        nextLineData.Attack = 0;
        nextLineData.Speed = 0;
        nextLineData.Defence = 0;
        nextLineData.Medicine = 0;
        nextLineData.UsePoison = 0;
        nextLineData.Detoxification = 0;
        nextLineData.AntiPoison = 0;
        nextLineData.Fist = 0;
        nextLineData.Sword = 0;
        nextLineData.Knife = 0;
        nextLineData.Unusual = 0;
        nextLineData.HiddenWeapon = 0;
        nextLineData.Knowledge = 0;
        nextLineData.Morality = 0;
        nextLineData.AttackWithPoison = 0;
        nextLineData.AttackTwice = 0;
        nextLineData.Fame = 0;
        nextLineData.IQ = 0;
        nextLineData.PracticeItem = -1;
        nextLineData.ExpForItem = 0;
        for (int j = 0; j < 10; j++)
        {
            nextLineData.MagicID[j] = 0;
        }
        for (int j = 0; j < 10; j++)
        {
            nextLineData.MagicLevel[j] = 0;
        }
        for (int j = 0; j < 4; j++)
        {
            nextLineData.TakingItem[j] = -1;
        }
        for (int j = 0; j < 4; j++)
        {
            nextLineData.TakingItemCount[j] = 0;
        }
        return nextLineData;
    };
    char* Name__;
    char* Nick__;
    int lines = 0;
    auto nextLineData = getDefault();
    while (in.read_row(
        nextLineData.ID,
        nextLineData.HeadID,
        nextLineData.IncLife,
        nextLineData.UnUse,
        Name__,
        Nick__,
        nextLineData.Sexual,
        nextLineData.Level,
        nextLineData.Exp,
        nextLineData.HP,
        nextLineData.MaxHP,
        nextLineData.Hurt,
        nextLineData.Poison,
        nextLineData.PhysicalPower,
        nextLineData.ExpForMakeItem,
        nextLineData.Equip0,
        nextLineData.Equip1,
        nextLineData.Frame[0],
        nextLineData.Frame[1],
        nextLineData.Frame[2],
        nextLineData.Frame[3],
        nextLineData.Frame[4],
        nextLineData.Frame[5],
        nextLineData.Frame[6],
        nextLineData.Frame[7],
        nextLineData.Frame[8],
        nextLineData.Frame[9],
        nextLineData.Frame[10],
        nextLineData.Frame[11],
        nextLineData.Frame[12],
        nextLineData.Frame[13],
        nextLineData.Frame[14],
        nextLineData.MPType,
        nextLineData.MP,
        nextLineData.MaxMP,
        nextLineData.Attack,
        nextLineData.Speed,
        nextLineData.Defence,
        nextLineData.Medicine,
        nextLineData.UsePoison,
        nextLineData.Detoxification,
        nextLineData.AntiPoison,
        nextLineData.Fist,
        nextLineData.Sword,
        nextLineData.Knife,
        nextLineData.Unusual,
        nextLineData.HiddenWeapon,
        nextLineData.Knowledge,
        nextLineData.Morality,
        nextLineData.AttackWithPoison,
        nextLineData.AttackTwice,
        nextLineData.Fame,
        nextLineData.IQ,
        nextLineData.PracticeItem,
        nextLineData.ExpForItem,
        nextLineData.MagicID[0],
        nextLineData.MagicID[1],
        nextLineData.MagicID[2],
        nextLineData.MagicID[3],
        nextLineData.MagicID[4],
        nextLineData.MagicID[5],
        nextLineData.MagicID[6],
        nextLineData.MagicID[7],
        nextLineData.MagicID[8],
        nextLineData.MagicID[9],
        nextLineData.MagicLevel[0],
        nextLineData.MagicLevel[1],
        nextLineData.MagicLevel[2],
        nextLineData.MagicLevel[3],
        nextLineData.MagicLevel[4],
        nextLineData.MagicLevel[5],
        nextLineData.MagicLevel[6],
        nextLineData.MagicLevel[7],
        nextLineData.MagicLevel[8],
        nextLineData.MagicLevel[9],
        nextLineData.TakingItem[0],
        nextLineData.TakingItem[1],
        nextLineData.TakingItem[2],
        nextLineData.TakingItem[3],
        nextLineData.TakingItemCount[0],
        nextLineData.TakingItemCount[1],
        nextLineData.TakingItemCount[2],
        nextLineData.TakingItemCount[3]))
    {
        strncpy(nextLineData.Name, Name__, sizeof(nextLineData.Name) - 1);
        strncpy(nextLineData.Nick, Nick__, sizeof(nextLineData.Nick) - 1);
        data.push_back(nextLineData);
        lines++;
        nextLineData = getDefault();
    }
}
// ��Ʒ
void Save::NewSave::LoadFromCSVItemSave(std::vector<Item>& data, int record)
{
    data.clear();
    io::CSVReader<72, io::trim_chars<>, io::double_quote_escape<',', '\"'>> in("../game/save/csv/" + std::to_string(record) + "_��Ʒ.csv");
    in.read_header(io::ignore_missing_column | io::ignore_extra_column,
        "���",
        "��Ʒ��",
        "��Ʒ������1",
        "��Ʒ������2",
        "��Ʒ������3",
        "��Ʒ������4",
        "��Ʒ������5",
        "��Ʒ������6",
        "��Ʒ������7",
        "��Ʒ������8",
        "��Ʒ������9",
        "��Ʒ������10",
        "��Ʒ˵��",
        "�����书",
        "�����������",
        "ʹ����",
        "װ������",
        "��ʾ��Ʒ˵��",
        "��Ʒ����",
        "δ֪5",
        "δ֪6",
        "δ֪7",
        "������",
        "���������ֵ",
        "���ж��ⶾ",
        "������",
        "�ı���������",
        "������",
        "���������ֵ",
        "�ӹ�����",
        "���Ṧ",
        "�ӷ�����",
        "��ҽ��",
        "��ʹ��",
        "�ӽⶾ",
        "�ӿ���",
        "��ȭ��",
        "������",
        "��ˣ��",
        "���������",
        "�Ӱ�������",
        "����ѧ��ʶ",
        "��Ʒ��",
        "�����һ���",
        "�ӹ�������",
        "����������",
        "����������",
        "������",
        "�蹥����",
        "���Ṧ",
        "���ö�",
        "��ҽ��",
        "��ⶾ",
        "��ȭ��",
        "������",
        "��ˣ��",
        "���������",
        "�谵��",
        "������",
        "�辭��",
        "������Ʒ�辭��",
        "�����",
        "������Ʒ1",
        "������Ʒ2",
        "������Ʒ3",
        "������Ʒ4",
        "������Ʒ5",
        "������Ʒ����1",
        "������Ʒ����2",
        "������Ʒ����3",
        "������Ʒ����4",
        "������Ʒ����5");
    auto getDefault = []()
    {
        Item nextLineData;
        nextLineData.ID = 0;
        memset(nextLineData.Name, '\0', sizeof(nextLineData.Name));
        for (int j = 0; j < 10; j++)
        {
            nextLineData.Name1[j] = 0;
        }
        memset(nextLineData.Introduction, '\0', sizeof(nextLineData.Introduction));
        nextLineData.MagicID = -1;
        nextLineData.HiddenWeaponEffectID = -1;
        nextLineData.User = -1;
        nextLineData.EquipType = -1;
        nextLineData.ShowIntroduction = 0;
        nextLineData.ItemType = 0;
        nextLineData.UnKnown5 = 0;
        nextLineData.UnKnown6 = 0;
        nextLineData.UnKnown7 = 0;
        nextLineData.AddHP = 0;
        nextLineData.AddMaxHP = 0;
        nextLineData.AddPoison = 0;
        nextLineData.AddPhysicalPower = 0;
        nextLineData.ChangeMPType = -1;
        nextLineData.AddMP = 0;
        nextLineData.AddMaxMP = 0;
        nextLineData.AddAttack = 0;
        nextLineData.AddSpeed = 0;
        nextLineData.AddDefence = 0;
        nextLineData.AddMedicine = 0;
        nextLineData.AddUsePoison = 0;
        nextLineData.AddDetoxification = 0;
        nextLineData.AddAntiPoison = 0;
        nextLineData.AddFist = 0;
        nextLineData.AddSword = 0;
        nextLineData.AddKnife = 0;
        nextLineData.AddUnusual = 0;
        nextLineData.AddHiddenWeapon = 0;
        nextLineData.AddKnowledge = 0;
        nextLineData.AddMorality = 0;
        nextLineData.AddAttackTwice = 0;
        nextLineData.AddAttackWithPoison = 0;
        nextLineData.OnlySuitableRole = -1;
        nextLineData.NeedMPType = 0;
        nextLineData.NeedMP = 0;
        nextLineData.NeedAttack = 0;
        nextLineData.NeedSpeed = 0;
        nextLineData.NeedUsePoison = 0;
        nextLineData.NeedMedicine = 0;
        nextLineData.NeedDetoxification = 0;
        nextLineData.NeedFist = 0;
        nextLineData.NeedSword = 0;
        nextLineData.NeedKnife = 0;
        nextLineData.NeedUnusual = 0;
        nextLineData.NeedHiddenWeapon = 0;
        nextLineData.NeedIQ = 0;
        nextLineData.NeedExp = 0;
        nextLineData.NeedExpForMakeItem = 0;
        nextLineData.NeedMaterial = -1;
        for (int j = 0; j < 5; j++)
        {
            nextLineData.MakeItem[j] = -1;
        }
        for (int j = 0; j < 5; j++)
        {
            nextLineData.MakeItemCount[j] = 0;
        }
        return nextLineData;
    };
    char* Name__;
    char* Introduction__;
    int lines = 0;
    auto nextLineData = getDefault();
    while (in.read_row(
        nextLineData.ID,
        Name__,
        nextLineData.Name1[0],
        nextLineData.Name1[1],
        nextLineData.Name1[2],
        nextLineData.Name1[3],
        nextLineData.Name1[4],
        nextLineData.Name1[5],
        nextLineData.Name1[6],
        nextLineData.Name1[7],
        nextLineData.Name1[8],
        nextLineData.Name1[9],
        Introduction__,
        nextLineData.MagicID,
        nextLineData.HiddenWeaponEffectID,
        nextLineData.User,
        nextLineData.EquipType,
        nextLineData.ShowIntroduction,
        nextLineData.ItemType,
        nextLineData.UnKnown5,
        nextLineData.UnKnown6,
        nextLineData.UnKnown7,
        nextLineData.AddHP,
        nextLineData.AddMaxHP,
        nextLineData.AddPoison,
        nextLineData.AddPhysicalPower,
        nextLineData.ChangeMPType,
        nextLineData.AddMP,
        nextLineData.AddMaxMP,
        nextLineData.AddAttack,
        nextLineData.AddSpeed,
        nextLineData.AddDefence,
        nextLineData.AddMedicine,
        nextLineData.AddUsePoison,
        nextLineData.AddDetoxification,
        nextLineData.AddAntiPoison,
        nextLineData.AddFist,
        nextLineData.AddSword,
        nextLineData.AddKnife,
        nextLineData.AddUnusual,
        nextLineData.AddHiddenWeapon,
        nextLineData.AddKnowledge,
        nextLineData.AddMorality,
        nextLineData.AddAttackTwice,
        nextLineData.AddAttackWithPoison,
        nextLineData.OnlySuitableRole,
        nextLineData.NeedMPType,
        nextLineData.NeedMP,
        nextLineData.NeedAttack,
        nextLineData.NeedSpeed,
        nextLineData.NeedUsePoison,
        nextLineData.NeedMedicine,
        nextLineData.NeedDetoxification,
        nextLineData.NeedFist,
        nextLineData.NeedSword,
        nextLineData.NeedKnife,
        nextLineData.NeedUnusual,
        nextLineData.NeedHiddenWeapon,
        nextLineData.NeedIQ,
        nextLineData.NeedExp,
        nextLineData.NeedExpForMakeItem,
        nextLineData.NeedMaterial,
        nextLineData.MakeItem[0],
        nextLineData.MakeItem[1],
        nextLineData.MakeItem[2],
        nextLineData.MakeItem[3],
        nextLineData.MakeItem[4],
        nextLineData.MakeItemCount[0],
        nextLineData.MakeItemCount[1],
        nextLineData.MakeItemCount[2],
        nextLineData.MakeItemCount[3],
        nextLineData.MakeItemCount[4]))
    {
        strncpy(nextLineData.Introduction, Introduction__, sizeof(nextLineData.Introduction) - 1);
        strncpy(nextLineData.Name, Name__, sizeof(nextLineData.Name) - 1);
        data.push_back(nextLineData);
        lines++;
        nextLineData = getDefault();
    }
}
// ����
void Save::NewSave::LoadFromCSVSubMapInfoSave(std::vector<SubMapInfo>& data, int record)
{
    data.clear();
    io::CSVReader<22, io::trim_chars<>, io::double_quote_escape<',', '\"'>> in("../game/save/csv/" + std::to_string(record) + "_����.csv");
    in.read_header(io::ignore_missing_column | io::ignore_extra_column,
        "���",
        "����",
        "��������",
        "��������",
        "��ת����",
        "��������",
        "�⾰���X1",
        "�⾰���Y1",
        "�⾰���X2",
        "�⾰���Y2",
        "���X",
        "���Y",
        "����X1",
        "����X2",
        "����X3",
        "����Y1",
        "����Y2",
        "����Y3",
        "��תX",
        "��תY",
        "��ת����X",
        "��ת����Y");
    auto getDefault = []()
    {
        SubMapInfo nextLineData;
        nextLineData.ID = 0;
        memset(nextLineData.Name, '\0', sizeof(nextLineData.Name));
        nextLineData.ExitMusic = 0;
        nextLineData.EntranceMusic = -1;
        nextLineData.JumpSubMap = -1;
        nextLineData.EntranceCondition = 0;
        nextLineData.MainEntranceX1 = 0;
        nextLineData.MainEntranceY1 = 0;
        nextLineData.MainEntranceX2 = 0;
        nextLineData.MainEntranceY2 = 0;
        nextLineData.EntranceX = 0;
        nextLineData.EntranceY = 0;
        for (int j = 0; j < 3; j++)
        {
            nextLineData.ExitX[j] = -1;
        }
        for (int j = 0; j < 3; j++)
        {
            nextLineData.ExitY[j] = -1;
        }
        nextLineData.JumpX = 0;
        nextLineData.JumpY = 0;
        nextLineData.JumpReturnX = 0;
        nextLineData.JumpReturnY = 0;
        return nextLineData;
    };
    char* Name__;
    int lines = 0;
    auto nextLineData = getDefault();
    while (in.read_row(
        nextLineData.ID,
        Name__,
        nextLineData.ExitMusic,
        nextLineData.EntranceMusic,
        nextLineData.JumpSubMap,
        nextLineData.EntranceCondition,
        nextLineData.MainEntranceX1,
        nextLineData.MainEntranceY1,
        nextLineData.MainEntranceX2,
        nextLineData.MainEntranceY2,
        nextLineData.EntranceX,
        nextLineData.EntranceY,
        nextLineData.ExitX[0],
        nextLineData.ExitX[1],
        nextLineData.ExitX[2],
        nextLineData.ExitY[0],
        nextLineData.ExitY[1],
        nextLineData.ExitY[2],
        nextLineData.JumpX,
        nextLineData.JumpY,
        nextLineData.JumpReturnX,
        nextLineData.JumpReturnY))
    {
        strncpy(nextLineData.Name, Name__, sizeof(nextLineData.Name) - 1);
        data.push_back(nextLineData);
        lines++;
        nextLineData = getDefault();
    }
}
// �书
void Save::NewSave::LoadFromCSVMagicSave(std::vector<Magic>& data, int record)
{
    data.clear();
    io::CSVReader<64, io::trim_chars<>, io::double_quote_escape<',', '\"'>> in("../game/save/csv/" + std::to_string(record) + "_�书.csv");
    in.read_header(io::ignore_missing_column | io::ignore_extra_column,
        "���",
        "����",
        "δ֪1",
        "δ֪2",
        "δ֪3",
        "δ֪4",
        "δ֪5",
        "������Ч",
        "�书����",
        "�书����",
        "�˺�����",
        "������Χ����",
        "��������",
        "�����ж�",
        "����1",
        "����2",
        "����3",
        "����4",
        "����5",
        "����6",
        "����7",
        "����8",
        "����9",
        "����10",
        "�ƶ���Χ1",
        "�ƶ���Χ2",
        "�ƶ���Χ3",
        "�ƶ���Χ4",
        "�ƶ���Χ5",
        "�ƶ���Χ6",
        "�ƶ���Χ7",
        "�ƶ���Χ8",
        "�ƶ���Χ9",
        "�ƶ���Χ10",
        "ɱ�˷�Χ1",
        "ɱ�˷�Χ2",
        "ɱ�˷�Χ3",
        "ɱ�˷�Χ4",
        "ɱ�˷�Χ5",
        "ɱ�˷�Χ6",
        "ɱ�˷�Χ7",
        "ɱ�˷�Χ8",
        "ɱ�˷�Χ9",
        "ɱ�˷�Χ10",
        "������1",
        "������2",
        "������3",
        "������4",
        "������5",
        "������6",
        "������7",
        "������8",
        "������9",
        "������10",
        "ɱ������1",
        "ɱ������2",
        "ɱ������3",
        "ɱ������4",
        "ɱ������5",
        "ɱ������6",
        "ɱ������7",
        "ɱ������8",
        "ɱ������9",
        "ɱ������10");
    auto getDefault = []()
    {
        Magic nextLineData;
        nextLineData.ID = 0;
        memset(nextLineData.Name, '\0', sizeof(nextLineData.Name));
        for (int j = 0; j < 5; j++)
        {
            nextLineData.Unknown[j] = 0;
        }
        nextLineData.SoundID = 0;
        nextLineData.MagicType = 0;
        nextLineData.EffectID = 0;
        nextLineData.HurtType = 0;
        nextLineData.AttackAreaType = 0;
        nextLineData.NeedMP = 0;
        nextLineData.WithPoison = 0;
        for (int j = 0; j < 10; j++)
        {
            nextLineData.Attack[j] = 0;
        }
        for (int j = 0; j < 10; j++)
        {
            nextLineData.SelectDistance[j] = 0;
        }
        for (int j = 0; j < 10; j++)
        {
            nextLineData.AttackDistance[j] = 0;
        }
        for (int j = 0; j < 10; j++)
        {
            nextLineData.AddMP[j] = 0;
        }
        for (int j = 0; j < 10; j++)
        {
            nextLineData.HurtMP[j] = 0;
        }
        return nextLineData;
    };
    char* Name__;
    int lines = 0;
    auto nextLineData = getDefault();
    while (in.read_row(
        nextLineData.ID,
        Name__,
        nextLineData.Unknown[0],
        nextLineData.Unknown[1],
        nextLineData.Unknown[2],
        nextLineData.Unknown[3],
        nextLineData.Unknown[4],
        nextLineData.SoundID,
        nextLineData.MagicType,
        nextLineData.EffectID,
        nextLineData.HurtType,
        nextLineData.AttackAreaType,
        nextLineData.NeedMP,
        nextLineData.WithPoison,
        nextLineData.Attack[0],
        nextLineData.Attack[1],
        nextLineData.Attack[2],
        nextLineData.Attack[3],
        nextLineData.Attack[4],
        nextLineData.Attack[5],
        nextLineData.Attack[6],
        nextLineData.Attack[7],
        nextLineData.Attack[8],
        nextLineData.Attack[9],
        nextLineData.SelectDistance[0],
        nextLineData.SelectDistance[1],
        nextLineData.SelectDistance[2],
        nextLineData.SelectDistance[3],
        nextLineData.SelectDistance[4],
        nextLineData.SelectDistance[5],
        nextLineData.SelectDistance[6],
        nextLineData.SelectDistance[7],
        nextLineData.SelectDistance[8],
        nextLineData.SelectDistance[9],
        nextLineData.AttackDistance[0],
        nextLineData.AttackDistance[1],
        nextLineData.AttackDistance[2],
        nextLineData.AttackDistance[3],
        nextLineData.AttackDistance[4],
        nextLineData.AttackDistance[5],
        nextLineData.AttackDistance[6],
        nextLineData.AttackDistance[7],
        nextLineData.AttackDistance[8],
        nextLineData.AttackDistance[9],
        nextLineData.AddMP[0],
        nextLineData.AddMP[1],
        nextLineData.AddMP[2],
        nextLineData.AddMP[3],
        nextLineData.AddMP[4],
        nextLineData.AddMP[5],
        nextLineData.AddMP[6],
        nextLineData.AddMP[7],
        nextLineData.AddMP[8],
        nextLineData.AddMP[9],
        nextLineData.HurtMP[0],
        nextLineData.HurtMP[1],
        nextLineData.HurtMP[2],
        nextLineData.HurtMP[3],
        nextLineData.HurtMP[4],
        nextLineData.HurtMP[5],
        nextLineData.HurtMP[6],
        nextLineData.HurtMP[7],
        nextLineData.HurtMP[8],
        nextLineData.HurtMP[9]))
    {
        strncpy(nextLineData.Name, Name__, sizeof(nextLineData.Name) - 1);
        data.push_back(nextLineData);
        lines++;
        nextLineData = getDefault();
    }
}
// �̵�
void Save::NewSave::LoadFromCSVShopSave(std::vector<Shop>& data, int record)
{
    data.clear();
    io::CSVReader<15, io::trim_chars<>, io::double_quote_escape<',', '\"'>> in("../game/save/csv/" + std::to_string(record) + "_�̵�.csv");
    in.read_header(io::ignore_missing_column | io::ignore_extra_column,
        "��Ʒ���1",
        "��Ʒ���2",
        "��Ʒ���3",
        "��Ʒ���4",
        "��Ʒ���5",
        "��Ʒ����1",
        "��Ʒ����2",
        "��Ʒ����3",
        "��Ʒ����4",
        "��Ʒ����5",
        "��Ʒ�۸�1",
        "��Ʒ�۸�2",
        "��Ʒ�۸�3",
        "��Ʒ�۸�4",
        "��Ʒ�۸�5");
    auto getDefault = []()
    {
        Shop nextLineData;
        for (int j = 0; j < 5; j++)
        {
            nextLineData.ItemID[j] = -1;
        }
        for (int j = 0; j < 5; j++)
        {
            nextLineData.Total[j] = 0;
        }
        for (int j = 0; j < 5; j++)
        {
            nextLineData.Price[j] = 0;
        }
        return nextLineData;
    };
    int lines = 0;
    auto nextLineData = getDefault();
    while (in.read_row(
        nextLineData.ItemID[0],
        nextLineData.ItemID[1],
        nextLineData.ItemID[2],
        nextLineData.ItemID[3],
        nextLineData.ItemID[4],
        nextLineData.Total[0],
        nextLineData.Total[1],
        nextLineData.Total[2],
        nextLineData.Total[3],
        nextLineData.Total[4],
        nextLineData.Price[0],
        nextLineData.Price[1],
        nextLineData.Price[2],
        nextLineData.Price[3],
        nextLineData.Price[4]))
    {
        data.push_back(nextLineData);
        lines++;
        nextLineData = getDefault();
    }
}
void Save::NewSave::InsertRoleAt(std::vector<Role>& data, int idx)
{
    auto newCopy = data[idx];
    data.insert(data.begin() + idx, newCopy);
    for (int i = 0; i < data.size(); i++)
    {
        data[i].ID = i;
    }
    Save::getInstance()->updateAllPtrVector();
    for (int i = 0; i < 1; i++)
    {
        auto p = Save::getInstance();
        if (p->Team[0] >= idx)
        { p->Team[0] += 1; }
        if (p->Team[1] >= idx)
        { p->Team[1] += 1; }
        if (p->Team[2] >= idx)
        { p->Team[2] += 1; }
        if (p->Team[3] >= idx)
        { p->Team[3] += 1; }
        if (p->Team[4] >= idx)
        { p->Team[4] += 1; }
        if (p->Team[5] >= idx)
        { p->Team[5] += 1; }
    }
    for (auto& p : Save::getInstance()->getItems())
    {
        if (p->User >= idx)
        { p->User += 1; }
        if (p->OnlySuitableRole >= idx)
        { p->OnlySuitableRole += 1; }
    }
}
void Save::NewSave::InsertItemAt(std::vector<Item>& data, int idx)
{
    auto newCopy = data[idx];
    data.insert(data.begin() + idx, newCopy);
    for (int i = 0; i < data.size(); i++)
    {
        data[i].ID = i;
    }
    Save::getInstance()->updateAllPtrVector();
    for (int i = 0; i < ITEM_IN_BAG_COUNT; i++)
    {
        auto* p = &(Save::getInstance()->Items[i]);
        if (p->item_id >= idx)
        { p->item_id += 1; }
    }
    for (auto& p : Save::getInstance()->getRoles())
    {
        if (p->Equip0 >= idx)
        { p->Equip0 += 1; }
        if (p->Equip1 >= idx)
        { p->Equip1 += 1; }
        if (p->PracticeItem >= idx)
        { p->PracticeItem += 1; }
        if (p->TakingItem[0] >= idx)
        { p->TakingItem[0] += 1; }
        if (p->TakingItem[1] >= idx)
        { p->TakingItem[1] += 1; }
        if (p->TakingItem[2] >= idx)
        { p->TakingItem[2] += 1; }
        if (p->TakingItem[3] >= idx)
        { p->TakingItem[3] += 1; }
    }
    for (auto& p : Save::getInstance()->getItems())
    {
        if (p->NeedMaterial >= idx)
        { p->NeedMaterial += 1; }
        if (p->MakeItem[0] >= idx)
        { p->MakeItem[0] += 1; }
        if (p->MakeItem[1] >= idx)
        { p->MakeItem[1] += 1; }
        if (p->MakeItem[2] >= idx)
        { p->MakeItem[2] += 1; }
        if (p->MakeItem[3] >= idx)
        { p->MakeItem[3] += 1; }
        if (p->MakeItem[4] >= idx)
        { p->MakeItem[4] += 1; }
    }
    for (auto& p : Save::getInstance()->getShops())
    {
        if (p->ItemID[0] >= idx)
        { p->ItemID[0] += 1; }
        if (p->ItemID[1] >= idx)
        { p->ItemID[1] += 1; }
        if (p->ItemID[2] >= idx)
        { p->ItemID[2] += 1; }
        if (p->ItemID[3] >= idx)
        { p->ItemID[3] += 1; }
        if (p->ItemID[4] >= idx)
        { p->ItemID[4] += 1; }
    }
}
void Save::NewSave::InsertSubMapInfoAt(std::vector<SubMapInfo>& data, int idx)
{
    auto newCopy = data[idx];
    data.insert(data.begin() + idx, newCopy);
    for (int i = 0; i < data.size(); i++)
    {
        data[i].ID = i;
    }
    Save::getInstance()->updateAllPtrVector();
}
void Save::NewSave::InsertMagicAt(std::vector<Magic>& data, int idx)
{
    auto newCopy = data[idx];
    data.insert(data.begin() + idx, newCopy);
    for (int i = 0; i < data.size(); i++)
    {
        data[i].ID = i;
    }
    Save::getInstance()->updateAllPtrVector();
    for (auto& p : Save::getInstance()->getRoles())
    {
        if (p->MagicID[0] >= idx)
        { p->MagicID[0] += 1; }
        if (p->MagicID[1] >= idx)
        { p->MagicID[1] += 1; }
        if (p->MagicID[2] >= idx)
        { p->MagicID[2] += 1; }
        if (p->MagicID[3] >= idx)
        { p->MagicID[3] += 1; }
        if (p->MagicID[4] >= idx)
        { p->MagicID[4] += 1; }
        if (p->MagicID[5] >= idx)
        { p->MagicID[5] += 1; }
        if (p->MagicID[6] >= idx)
        { p->MagicID[6] += 1; }
        if (p->MagicID[7] >= idx)
        { p->MagicID[7] += 1; }
        if (p->MagicID[8] >= idx)
        { p->MagicID[8] += 1; }
        if (p->MagicID[9] >= idx)
        { p->MagicID[9] += 1; }
    }
    for (auto& p : Save::getInstance()->getItems())
    {
        if (p->MagicID >= idx)
        { p->MagicID += 1; }
    }
}
void Save::NewSave::InsertShopAt(std::vector<Shop>& data, int idx)
{
    auto newCopy = data[idx];
    data.insert(data.begin() + idx, newCopy);
    for (int i = 0; i < data.size(); i++)
    {
        // data[i].ID = i;
    }
    Save::getInstance()->updateAllPtrVector();
}