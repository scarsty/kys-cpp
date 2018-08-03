#include "Audio.h"
#include "BattleScene.h"
#include "Event.h"
#include "File.h"
#include "Font.h"
#include "GameUtil.h"
#include "MainScene.h"
#include "Menu.h"
#include "PotConv.h"
#include "Random.h"
#include "Save.h"
#include "ShowExp.h"
#include "ShowRoleDifference.h"
#include "TeamMenu.h"
#include "UIStatus.h"
#include "Util.h"
#include <algorithm>
#include <iostream>
#include <math.h>
#include <string>

BattleScene::BattleScene()
{
    full_window_ = 1;
    COORD_COUNT = BATTLEMAP_COORD_COUNT;

    earth_layer_ = new MapSquareInt(COORD_COUNT);
    building_layer_ = new MapSquareInt(COORD_COUNT);
    select_layer_ = new MapSquareInt(COORD_COUNT);
    effect_layer_ = new MapSquareInt(COORD_COUNT);
    battle_menu_ = new BattleActionMenu();

    role_layer_ = new MapSquare<Role*>(COORD_COUNT);

    battle_menu_->setBattleScene(this);
    battle_menu_->setPosition(160, 200);
    head_self_ = new Head();
    addChild(head_self_);
    battle_cursor_ = new BattleCursor();
    battle_cursor_->setBattleScene(this);
    save_ = Save::getInstance();
    semi_real_ = GameUtil::getInstance()->getInt("game", "semi_real", 0);
}

BattleScene::BattleScene(int id)
    : BattleScene()
{
    setID(id);
}

BattleScene::~BattleScene()
{
    delete battle_menu_;
    delete battle_cursor_;
    Util::safe_delete({ &earth_layer_, &building_layer_, &select_layer_, &effect_layer_ });
    delete role_layer_;
}

void BattleScene::setID(int id)
{
    battle_id_ = id;
    info_ = BattleMap::getInstance()->getBattleInfo(id);

    BattleMap::getInstance()->copyLayerData(info_->BattleFieldID, 0, earth_layer_);
    BattleMap::getInstance()->copyLayerData(info_->BattleFieldID, 1, building_layer_);

    role_layer_->setAll(nullptr);
    select_layer_->setAll(-1);
    effect_layer_->setAll(-1);
}

void BattleScene::draw()
{
    Engine::getInstance()->setRenderAssistTexture();
    Engine::getInstance()->fillColor({ 0, 0, 0, 255 }, 0, 0, render_center_x_ * 2, render_center_y_ * 2);
#ifndef _DEBUG
    for (int sum = -view_sum_region_; sum <= view_sum_region_ + 15; sum++)
    {
        for (int i = -view_width_region_; i <= view_width_region_; i++)
        {
            int ix = man_x_ + i + (sum / 2);
            int iy = man_y_ - i + (sum - sum / 2);
            auto p = getPositionOnRender(ix, iy, man_x_, man_y_);
            p.x += x_;
            p.y += y_;
            if (!isOutLine(ix, iy))
            {
                int num = earth_layer_->data(ix, iy) / 2;
                BP_Color color = { 255, 255, 255, 255 };
                if (battle_cursor_->isRunning() && !acting_role_->isAuto())    //������Զ�����û�б䰵��ѡ��Ч������̫��
                {
                    if (select_layer_->data(ix, iy) < 0)
                    {
                        color = { 64, 64, 64, 255 };
                    }
                    else
                    {
                        color = { 128, 128, 128, 255 };
                    }
                    if (battle_cursor_->getMode() == BattleCursor::Action)
                    {
                        if (haveEffect(ix, iy))
                        {
                            if (!canSelect(ix, iy))
                            {
                                color = { 160, 160, 160, 255 };
                            }
                            else
                            {
                                color = { 192, 192, 192, 255 };
                            }
                        }
                    }
                    if (ix == select_x_ && iy == select_y_)
                    {
                        color = { 255, 255, 255, 255 };
                    }
                }
                if (num > 0)
                {
                    TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y, color);
                }
            }
        }
    }
#endif
    for (int sum = -view_sum_region_; sum <= view_sum_region_ + 15; sum++)
    {
        for (int i = -view_width_region_; i <= view_width_region_; i++)
        {
            int ix = man_x_ + i + (sum / 2);
            int iy = man_y_ - i + (sum - sum / 2);
            auto p = getPositionOnRender(ix, iy, man_x_, man_y_);
            p.x += x_;
            p.y += y_;
            if (!isOutLine(ix, iy))
            {
                int num = building_layer_->data(ix, iy) / 2;
                if (num > 0)
                {
                    TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y);
                }
                auto r = role_layer_->data(ix, iy);
                if (r)
                {
                    std::string path = convert::formatString("fight/fight%03d", r->HeadID);
                    BP_Color color = { 255, 255, 255, 255 };
                    uint8_t alpha = 255;
                    if (battle_cursor_->isRunning() && !acting_role_->isAuto())
                    {
                        color = { 128, 128, 128, 255 };
                        if (inEffect(acting_role_, r))
                        {
                            color = { 255, 255, 255, 255 };
                        }
                    }
                    int pic;
                    if (r == acting_role_)
                    {
                        pic = calRolePic(r, action_type_, action_frame_);
                    }
                    else
                    {
                        pic = calRolePic(r);
                    }
                    if (r->HP <= 0)
                    {
                        alpha = dead_alpha_;
                    }
                    TextureManager::getInstance()->renderTexture(path, pic, p.x, p.y, color, alpha);
                }
                if (effect_id_ >= 0 && haveEffect(ix, iy))
                {
                    std::string path = convert::formatString("eft/eft%03d", effect_id_);
                    int dis = calDistance(acting_role_->X(), acting_role_->Y(), ix, iy);
                    num = effect_frame_ - dis + rand_.rand_int(3) - rand_.rand_int(3);
                    TextureManager::getInstance()->renderTexture(path, num, p.x, p.y, { 255, 255, 255, 255 }, 224);
                }
            }
        }
    }
    Engine::getInstance()->renderAssistTextureToWindow();

    if (semi_real_)
    {
        int h = Engine::getInstance()->getWindowHeight();
        TextureManager::getInstance()->renderTexture("title", 202, 200, h - 100);
        for (auto r : battle_roles_)
        {
            int x = 300 + r->Progress / 2;
            uint8_t alpha = 255;
            if (r->HP <= 0)
            {
                alpha = dead_alpha_;
            }
            TextureManager::getInstance()->renderTexture("head", r->HeadID, x, h - 100, { 255, 255, 255, 255 }, alpha, 0.25, 0.25);
        }
    }

    if (result_ >= 0)
    {
        Engine::getInstance()->fillColor({ 0, 0, 0, 128 }, 0, 0, -1, -1);
    }

    //printf("Battle scene drawn\n");
}

void BattleScene::dealEvent(BP_Event& e)
{
    if (battle_roles_.empty())
    {
        exitWithResult(0);
    }

    Role* role = nullptr;
    if (semi_real_ == 0)
    {
        //ѡ��λ�����������еĵ�һ����
        role = battle_roles_[0];

        //����һ�����Ѿ��ж�����˵�������˶��ж��ˣ�������ж�״̬����������
        if (role->Acted != 0)
        {
            resetRolesAct();
            sortRoles();
            role = battle_roles_[0];
        }
    }
    else
    {
        //������ͼѡ��һ��
        for (auto r : battle_roles_)
        {
            if (r->Progress > 1000)
            {
                role = r;
                break;
            }
        }
        //�޷�ѡ���ˣ����������˽�����������
        if (role == nullptr)
        {
            for (auto r : battle_roles_)
            {
                r->Progress += r->Speed / 4;
            }
            return;
        }
    }

    acting_role_ = role;

    //��λ
    man_x_ = role->X();
    man_y_ = role->Y();
    select_x_ = role->X();
    select_y_ = role->Y();
    head_self_->setRole(role);
    head_self_->setState(Element::Pass);

    //�ж�
    action(role);

    //������˳ɹ��ж�������ŵ���β
    if (role->Acted)
    {
        if (semi_real_ == 0)
        {
            battle_roles_.erase(battle_roles_.begin());
            battle_roles_.push_back(role);
        }
        else
        {
            role->Progress -= 1000;
            resetRolesAct();
        }
        poisonEffect(role);
    }

    //��������˵�����
    clearDead();

    //���ս�����
    int battle_result = checkResult();

    //�ҷ�ʤ
    if (battle_result >= 0)
    {
        result_ = battle_result;
        if (result_ == 0 || result_ == 1 && fail_exp_)
        {
            calExpGot();
        }
        setExit(true);
    }
}

void BattleScene::dealEvent2(BP_Event& e)
{
    if (isPressCancel(e))
    {
        for (auto r : battle_roles_)
        {
            if (r->Team == 0 && r->Auto == 1)    //ע�⣺autoΪ����ֵ�Ĳ���ȡ��
            {
                r->Auto = 0;
            }
        }
    }
}

void BattleScene::onEntrance()
{
    calViewRegion();
    Audio::getInstance()->playMusic(info_->Music);
    //ע���ʱ���ܵõ����ڵĴ�С����������ͷ���λ��
    head_self_->setPosition(80, 100);

    readBattleInfo();
    //��ʼ״̬
    for (auto r : battle_roles_)
    {
        setRoleInitState(r);
        r->Progress = 0;
    }
    //����
    sortRoles();
    acting_role_ = battle_roles_[0];
    //if (MainScene::getIntance()->inNorth())
    //{
    //    auto c1 = ParticleCreator::create("snow");
    //    addChild(c1);
    //    c1->init();
    //    c1->setPosition(512, 0);
    //    c1->setPosVar({ 512, 0 });
    //}
    Element::addOnRootTop(MainScene::getInstance()->getWeather());
}

void BattleScene::onExit()
{
    //���ȫ����ɫ��λ�ò�
    for (auto r : Save::getInstance()->getRoles())
    {
        r->setRolePositionLayer(nullptr);
    }
    Element::removeFromRoot(MainScene::getInstance()->getWeather());
}

void BattleScene::backRun()
{
}

//��ȡս����Ϣ��ȷ����ѡ���ﻹ���Զ�����
void BattleScene::readBattleInfo()
{
    //����ȫ����ɫ��λ�ò㣬���������
    for (auto r : Save::getInstance()->getRoles())
    {
        r->setRolePositionLayer(role_layer_);
        r->Team = 2;    //��ȫ�����óɲ����ڵ���Ӫ
        r->Auto = 1;
    }
    //��������λ�ú���Ӫ�������ĺ���ͳһ����
    //�з�
    for (int i = 0; i < BATTLE_ENEMY_COUNT; i++)
    {
        auto r = Save::getInstance()->getRole(info_->Enemy[i]);
        if (r)
        {
            battle_roles_.push_back(r);
            r->setPosition(info_->EnemyX[i], info_->EnemyY[i]);
            r->Team = 1;
            readFightFrame(r);
            r->FaceTowards = rand_.rand_int(4);
        }
    }

    //�ӽ�ת����һ������
    if (battle_roles_.size() > 0)
    {
        man_x_ = battle_roles_[0]->X();
        man_y_ = battle_roles_[0]->Y();
    }
    else
    {
        man_x_ = COORD_COUNT / 2;
        man_y_ = COORD_COUNT / 2;
    }

    //�ж��ǲ������Զ�ս������
    if (info_->AutoTeamMate[0] >= 0)
    {
        for (int i = 0; i < TEAMMATE_COUNT; i++)
        {
            auto r = Save::getInstance()->getRole(info_->AutoTeamMate[i]);
            if (r)
            {
                friends_.push_back(r);
                r->Auto = 2;    //��AI����
            }
        }
    }
    else
    {
        auto team_menu = new TeamMenu();
        team_menu->setMode(1);
        team_menu->run();
        friends_ = team_menu->getRoles();
        delete team_menu;
    }
    //����
    for (int i = 0; i < friends_.size(); i++)
    {
        auto r = friends_[i];
        if (r)
        {
            battle_roles_.push_back(r);
            r->setPosition(info_->TeamMateX[i], info_->TeamMateY[i]);
            r->Team = 0;
        }
    }
}

void BattleScene::setRoleInitState(Role* r)
{
    r->Acted = 0;
    r->ExpGot = 0;
    r->clearShowStrings();
    r->FightingFrame = 0;
    r->Moved = 0;
    r->AI_Action = -1;
    r->Effect = -1;
    r->BattleHurt = 0;

    if (r->Team == 0)
    {
        r->Auto = 0;
        GameUtil::limit2(r->HP, r->MaxHP / 10, r->MaxHP);
        GameUtil::limit2(r->MP, r->MaxMP / 10, r->MaxMP);
        ;
    }
    else
    {
        r->Auto = 1;
        //�з��лظ�״̬���Ŵ�
        r->PhysicalPower = 90;
        r->HP = r->MaxHP;
        r->MP = r->MaxMP;
        r->Poison = 0;
        r->Hurt = 0;
    }

    //��ȡ����֡��
    readFightFrame(r);

    setFaceTowardsNearest(r);
    //r->FaceTowards = RandomClassical::rand(4);  //ûͷ��Ӭ����ѡ������
}

void BattleScene::setFaceTowardsNearest(Role* r, bool in_effect /*= false*/)
{
    //Ѱ�����Լ�����ĵз�����������
    int min_distance = COORD_COUNT * COORD_COUNT;
    Role* r_near = nullptr;
    for (auto r1 : battle_roles_)
    {
        if (!in_effect && r->Team != r1->Team || in_effect && inEffect(r, r1))
        {
            int dis = calRoleDistance(r, r1);
            if (dis < min_distance)
            {
                r_near = r1;
                min_distance = dis;
            }
        }
    }
    if (r_near)
    {
        auto tw = calTowards(r->X(), r->Y(), r_near->X(), r_near->Y());
        if (tw != Towards_None)
        {
            r->FaceTowards = tw;
        }
    }
}

void BattleScene::readFightFrame(Role* r)
{
    if (r->FightFrame[0] >= 0)
    {
        return;
    }
    for (int i = 0; i < 5; i++)
    {
        r->FightFrame[i] = 0;
    }
    std::string file = convert::formatString("../game/resource/fight/fight%03d/fightframe.txt", r->HeadID);
    std::string frame_txt = convert::readStringFromFile(file);
    std::vector<int> frames;
    convert::findNumbers(frame_txt, &frames);
    for (int i = 0; i < frames.size() / 2; i++)
    {
        r->FightFrame[frames[i * 2]] = frames[i * 2 + 1];
    }
}

void BattleScene::sortRoles()
{
    if (semi_real_ == 0)
    {
        std::sort(battle_roles_.begin(), battle_roles_.end(), [](Role* r1, Role* r2) { return r1->Speed > r2->Speed; });
    }
    else
    {
        std::sort(battle_roles_.begin(), battle_roles_.end(), [](Role* r1, Role* r2) { return r1->Progress > r2->Progress; });
    }
}

void BattleScene::resetRolesAct()
{
    for (auto r : battle_roles_)
    {
        r->Acted = 0;
        r->Moved = 0;
        r->setPosition(r->X(), r->Y());
    }
}

int BattleScene::calMoveStep(Role* r)
{
    if (r->Moved)
    {
        return 0;
    }
    int speed = r->Speed;
    if (r->Equip0 >= 0)
    {
        auto i = Save::getInstance()->getItem(r->Equip0);
        speed += i->AddSpeed;
    }
    if (r->Equip1 >= 0)
    {
        auto i = Save::getInstance()->getItem(r->Equip1);
        speed += i->AddSpeed;
    }
    return speed / 15 + 1;
}

int BattleScene::calRolePic(Role* r, int style, int frame)
{
    if (r->FightFrame[style] <= 0)
    {
        style = -1;
    }
    if (style == -1)
    {
        for (int i = 0; i < 5; i++)
        {
            if (r->FightFrame[i] > 0)
            {
                return r->FightFrame[i] * r->FaceTowards;
            }
        }
    }
    else
    {
        int total = 0;
        for (int i = 0; i < 5; i++)
        {
            if (i == style)
            {
                return total + r->FightFrame[style] * r->FaceTowards + frame;
            }
            total += r->FightFrame[i] * 4;
        }
    }
    return r->FaceTowards;
}

void BattleScene::calSelectLayer(int x, int y, int team, int mode, int step /*= 0*/)
{
    if (mode == 0)
    {
        select_layer_->setAll(-1);
        std::vector<Point> cal_stack;
        select_layer_->data(x, y) = step;
        cal_stack.push_back({ x, y });
        int count = 0;
        while (step >= 0)
        {
            std::vector<Point> cal_stack_next;
            auto check_next = [&](Point p1) -> void
            {
                //δ������ҿ����ߵĸ��Ӳ�����һ���ļ���
                if (select_layer_->data(p1.x, p1.y) == -1 && canWalk(p1.x, p1.y))
                {
                    select_layer_->data(p1.x, p1.y) = step - 1;
                    cal_stack_next.push_back(p1);
                    count++;
                }
            };
            for (auto p : cal_stack)
            {
                //����Ƿ��ڵз����ԣ�������򿪴�ѡ��
                if (!isNearEnemy(team, p.x, p.y) || (p.x == x && p.y == y))
                {
                    //���4�����ڵ�
                    check_next({ p.x - 1, p.y });
                    check_next({ p.x + 1, p.y });
                    check_next({ p.x, p.y - 1 });
                    check_next({ p.x, p.y + 1 });
                }
                if (count >= COORD_COUNT * COORD_COUNT)
                {
                    break;
                }    //�������������������
            }
            if (cal_stack_next.size() == 0)
            {
                break;
            }    //���µĵ㣬����
            cal_stack = cal_stack_next;
            step--;
        }
    }
    else if (mode == 1)
    {
        for (int ix = 0; ix < COORD_COUNT; ix++)
        {
            for (int iy = 0; iy < COORD_COUNT; iy++)
            {
                select_layer_->data(ix, iy) = step - calDistance(ix, iy, x, y);
            }
        }
    }
    else if (mode == 2)
    {
        select_layer_->setAll(0);
    }
    else if (mode == 3)
    {
        for (int ix = 0; ix < COORD_COUNT; ix++)
        {
            for (int iy = 0; iy < COORD_COUNT; iy++)
            {
                int dx = abs(ix - x);
                int dy = abs(iy - y);
                if (dx == 0 && dy <= step || dy == 0 && dx <= step)
                {
                    select_layer_->data(ix, iy) = 0;
                }
                else
                {
                    select_layer_->data(ix, iy) = -1;
                }
            }
        }
        select_layer_->data(x, y) = -1;
    }
    else
    {
        select_layer_->setAll(-1);
        select_layer_->data(x, y) = 0;
    }
}

void BattleScene::calSelectLayer(Role* r, int mode, int step /*= 0*/)
{
    calSelectLayer(r->X(), r->Y(), r->Team, mode, step);
}

void BattleScene::calSelectLayerByMagic(int x, int y, int team, Magic* magic, int level_index)
{
    if (magic->AttackAreaType == 0 || magic->AttackAreaType == 3)
    {
        calSelectLayer(x, y, team, 1, magic->SelectDistance[level_index]);
    }
    else if (magic->AttackAreaType == 1)
    {
        calSelectLayer(x, y, team, 3, magic->SelectDistance[level_index]);
    }
    else
    {
        calSelectLayer(x, y, team, 4);
    }
}

void BattleScene::calEffectLayer(int x, int y, int select_x, int select_y, Magic* m /*= nullptr*/, int level_index /*= 0*/)
{
    effect_layer_->setAll(-1);

    //��δָ����ѧ������Ϊֻѡ��һ����
    if (m == nullptr || m->AttackAreaType == 0)
    {
        effect_layer_->data(select_x, select_y) = 0;
        return;
    }

    //�˴��Ƚ���׸����������
    if (m->AttackAreaType == 1)
    {
        int tw = calTowards(x, y, select_x, select_y);
        int dis = m->SelectDistance[level_index];
        for (int ix = x - dis; ix <= x + dis; ix++)
        {
            for (int iy = y - dis; iy <= y + dis; iy++)
            {
                if (!isOutLine(ix, iy) && (x == ix || y == iy) && calTowards(x, y, ix, iy) == tw)
                {
                    effect_layer_->data(ix, iy) = 0;
                }
            }
        }
    }
    else if (m->AttackAreaType == 2)
    {
        int dis = m->SelectDistance[level_index];
        for (int ix = x - dis; ix <= x + dis; ix++)
        {
            for (int iy = y - dis; iy <= y + dis; iy++)
            {
                if (!isOutLine(ix, iy) && (x == ix || y == iy))
                {
                    effect_layer_->data(ix, iy) = 0;
                }
            }
        }
    }
    else if (m->AttackAreaType == 3)
    {
        int dis = m->AttackDistance[level_index];
        for (int ix = select_x - dis; ix <= select_x + dis; ix++)
        {
            for (int iy = select_y - dis; iy <= select_y + dis; iy++)
            {
                if (!isOutLine(ix, iy))
                {
                    effect_layer_->data(ix, iy) = 0;
                }
            }
        }
    }
}

bool BattleScene::inEffect(Role* r1, Role* r2)
{
    if (haveEffect(r2->X(), r2->Y()))
    {
        if (r1->ActTeam == 0 && r1->Team == r2->Team)
        {
            return true;
        }
        else if (r1->ActTeam != 0 && r1->Team != r2->Team)
        {
            return true;
        }
    }
    return false;
}

bool BattleScene::canSelect(int x, int y)
{
    return (!isOutLine(x, y) && select_layer_->data(x, y) >= 0);
}

void BattleScene::walk(Role* r, int x, int y, Towards t)
{
    if (canWalk(x, y))
    {
        r->setPosition(x, y);
    }
}

bool BattleScene::canWalk(int x, int y)
{
    if (isOutLine(x, y) || isBuilding(x, y) || isWater(x, y) || isRole(x, y))
    {
        return false;
    }
    else
    {
        return true;
    }
}

bool BattleScene::isBuilding(int x, int y)
{
    return building_layer_->data(x, y) > 0;
}

bool BattleScene::isWater(int x, int y)
{
    int num = earth_layer_->data(x, y) / 2;
    if (num >= 179 && num <= 181 || num == 261 || num == 511 || num >= 662 && num <= 665 || num == 674)
    {
        return true;
    }
    return false;
}

bool BattleScene::isRole(int x, int y)
{
    return role_layer_->data(x, y);
}

bool BattleScene::isOutScreen(int x, int y)
{
    return (abs(man_x_ - x) >= 16 || abs(man_y_ - y) >= 20);
}

bool BattleScene::isNearEnemy(int team, int x, int y)
{
    for (auto r1 : battle_roles_)
    {
        if (team != r1->Team && calDistance(r1->X(), r1->Y(), x, y) <= 1)
        {
            return true;
        }
    }
    return false;
}

Role* BattleScene::getSelectedRole()
{
    return role_layer_->data(select_x_, select_y_);
}

void BattleScene::action(Role* r)
{
    battle_menu_->runAsRole(r);
    std::string str = battle_menu_->getResultString();

    //������������ͱ�ʾ���أ���������̫������
    if (str == "�Ƅ�")
    {
        actMove(r);
    }
    else if (str == "��W")
    {
        actUseMagic(r);
    }
    else if (str == "�ö�")
    {
        actUsePoison(r);
    }
    else if (str == "�ⶾ")
    {
        actDetoxification(r);
    }
    else if (str == "�t��")
    {
        actMedicine(r);
    }
    else if (str == "����")
    {
        actUseHiddenWeapon(r);
    }
    else if (str == "ˎƷ")
    {
        actUseDrag(r);
    }
    else if (str == "�ȴ�")
    {
        actWait(r);
    }
    else if (str == "��B")
    {
        actStatus(r);
    }
    else if (str == "�Ԅ�")
    {
        actAuto(r);
    }
    else if (str == "�Y��")
    {
        actRest(r);
    }

    //��һ���ж��ģ��˵���ӵ�һ����ʼ
    if (r->Acted)
    {
        battle_menu_->setStartItem(0);
    }
}

void BattleScene::actMove(Role* r)
{
    int step = calMoveStep(r);
    calSelectLayer(r, 0, step);
    battle_cursor_->setRoleAndMagic(r);
    battle_cursor_->setMode(BattleCursor::Move);
    if (battle_cursor_->run() == 0)
    {
        r->PhysicalPower = GameUtil::limit(r->PhysicalPower - 2, 0, Role::getMaxValue()->PhysicalPower);
        r->setPrevPosition(r->X(), r->Y());
        moveAnimation(r, select_x_, select_y_);
        r->ExpGot += 1;
        r->Moved = 1;
    }
}

void BattleScene::actUseMagic(Role* r)
{
    auto magic_menu = new BattleMagicMenu();
    while (true)
    {
        magic_menu->setStartItem(r->SelectedMagic);
        magic_menu->runAsRole(r);
        auto magic = magic_menu->getMagic();
        r->SelectedMagic = magic_menu->getResult();
        if (magic == nullptr)
        {
            break;
        }    //�������˳���Ϸ��������û��ѡ�书
        r->ActTeam = 1;
        //level_index��ʾ��0��9����level��0��999
        int level_index = r->getMagicLevelIndex(magic->ID);
        calSelectLayerByMagic(r->X(), r->Y(), r->Team, magic, level_index);
        //ѡ��Ŀ��
        battle_cursor_->setMode(BattleCursor::Action);
        battle_cursor_->setRoleAndMagic(r, magic, level_index);
        int selected = battle_cursor_->run();
        //ȡ��ѡ��Ŀ�������½���ѡ�书
        if (selected < 0)
        {
            continue;
        }
        else
        {
            for (int i = 0; i <= GameUtil::sign(r->AttackTwice); i++)
            {
                //���Ź������棬�����˺�
                showMagicName(magic->Name);
                r->PhysicalPower = GameUtil::limit(r->PhysicalPower - 3, 0, Role::getMaxValue()->PhysicalPower);
                r->MP = GameUtil::limit(r->MP - magic->calNeedMP(level_index), 0, r->MaxMP);
                useMagicAnimation(r, magic);
                calMagiclHurtAllEnemies(r, magic);
                showNumberAnimation();
                //��ѧ�ȼ�����
                auto index = r->getMagicOfRoleIndex(magic);
                if (index >= 0)
                {
                    r->MagicLevel[index] += 1 + rand_.rand_int(2);
                    GameUtil::limit2(r->MagicLevel[index], 0, MAX_MAGIC_LEVEL);
                }
                printf("%s %s level is %d\n", PotConv::to_read(r->Name).c_str(), PotConv::to_read(magic->Name).c_str(), r->MagicLevel[index]);
            }
            r->Acted = 1;
            break;
        }
    }
    delete magic_menu;
}

void BattleScene::actUsePoison(Role* r)
{
    calSelectLayer(r, 1, calActionStep(r->UsePoison));
    battle_cursor_->setMode(BattleCursor::Action);
    battle_cursor_->setRoleAndMagic(r);
    r->ActTeam = 1;
    int selected = battle_cursor_->run();
    if (selected >= 0)
    {
        auto r2 = getSelectedRole();
        if (r2)
        {
            int v = GameUtil::usePoison(r, r2);
            r2->addShowString(convert::formatString("%+d", v), { 20, 255, 20, 255 });
            r->ExpGot += v;
        }
        r->PhysicalPower = GameUtil::limit(r->PhysicalPower - 3, 0, Role::getMaxValue()->PhysicalPower);
        actionAnimation(r, 0, 30);
        showNumberAnimation();
        r->Acted = 1;
    }
}

void BattleScene::actDetoxification(Role* r)
{
    calSelectLayer(r, 1, calActionStep(r->Detoxification));
    battle_cursor_->setMode(BattleCursor::Action);
    battle_cursor_->setRoleAndMagic(r);
    r->ActTeam = 0;
    int selected = battle_cursor_->run();
    if (selected >= 0)
    {
        auto r2 = getSelectedRole();
        if (r2)
        {
            int v = GameUtil::detoxification(r, r2);
            r2->addShowString(convert::formatString("-%d", -v), { 20, 200, 255, 255 });
            r->ExpGot += v;
        }
        r->PhysicalPower = GameUtil::limit(r->PhysicalPower - 5, 0, Role::getMaxValue()->PhysicalPower);
        actionAnimation(r, 0, 36);
        showNumberAnimation();
        r->Acted = 1;
    }
}

void BattleScene::actMedicine(Role* r)
{
    calSelectLayer(r, 1, calActionStep(r->Medicine));
    battle_cursor_->setMode(BattleCursor::Action);
    battle_cursor_->setRoleAndMagic(r);
    r->ActTeam = 0;
    int selected = battle_cursor_->run();
    if (selected >= 0)
    {
        auto r2 = getSelectedRole();
        if (r2)
        {
            int v = GameUtil::medicine(r, r2);
            r2->addShowString(convert::formatString("-%d", abs(v)), { 255, 255, 200, 255 });
            r->ExpGot += v;
        }
        r->PhysicalPower = GameUtil::limit(r->PhysicalPower - 5, 0, Role::getMaxValue()->PhysicalPower);
        actionAnimation(r, 0, 0);
        showNumberAnimation();
        r->Acted = 1;
    }
}

void BattleScene::actUseHiddenWeapon(Role* r)
{
    auto item_menu = new BattleItemMenu();
    item_menu->setRole(r);
    item_menu->setForceItemType(3);
    item_menu->runAtPosition(300, 0);

    auto item = item_menu->getCurrentItem();
    if (item)
    {
        calSelectLayer(r, 1, calActionStep(r->HiddenWeapon));
        battle_cursor_->setMode(BattleCursor::Action);
        battle_cursor_->setRoleAndMagic(r);
        r->ActTeam = 1;
        int selected = battle_cursor_->run();
        if (selected >= 0)
        {
            auto r2 = getSelectedRole();
            int v = 0;
            if (r2)
            {
                v = calHiddenWeaponHurt(r, r2, item);
                r2->addShowString(convert::formatString("-%d", v), { 255, 20, 20, 255 });
            }
            showMagicName(item->Name);
            r->PhysicalPower = GameUtil::limit(r->PhysicalPower - 5, 0, Role::getMaxValue()->PhysicalPower);
            actionAnimation(r, 0, item->HiddenWeaponEffectID);
            if (r2)
            {
                r2->HP = GameUtil::limit(r2->HP - v, 0, r2->MaxHP);
            }
            showNumberAnimation();
            item_menu->addItem(item, -1);
            r->ExpGot += v;
            r->Acted = 1;
        }
    }
    delete item_menu;
}

void BattleScene::actUseDrag(Role* r)
{
    auto item_menu = new BattleItemMenu();
    item_menu->setForceItemType(2);
    item_menu->setRole(r);
    item_menu->runAtPosition(300, 0);

    auto item = item_menu->getCurrentItem();
    if (item)
    {
        Role r0 = *r;
        GameUtil::useItem(r, item);
        auto df = new ShowRoleDifference(&r0, r);
        df->setText(convert::formatString("%s����%s", r->Name, item->Name));
        df->setShowHead(false);
        df->setPosition(250, 220);
        df->setStayFrame(40);
        df->run();
        delete df;
        item_menu->addItem(item, -1);
        r->Acted = 1;
    }

    delete item_menu;
}

void BattleScene::actWait(Role* r)
{
    //�ȴ������Լ����뵽���һ��û�ж����˵ĺ���
    for (int i = 1; i < battle_roles_.size(); i++)
    {
        if (battle_roles_[i]->Acted == 0)
        {
            battle_roles_.erase(battle_roles_.begin());
            battle_roles_.insert(battle_roles_.begin() + i, r);
        }
    }
}

void BattleScene::actStatus(Role* r)
{
    head_self_->setVisible(false);
    battle_cursor_->getHead()->setVisible(false);
    battle_cursor_->getUIStatus()->setVisible(true);

    calSelectLayer(r, 2, 0);
    battle_cursor_->setRoleAndMagic(r);
    battle_cursor_->setMode(BattleCursor::Check);
    battle_cursor_->run();

    head_self_->setVisible(true);
    battle_cursor_->getHead()->setVisible(true);
    battle_cursor_->getUIStatus()->setVisible(false);
}

void BattleScene::actAuto(Role* r)
{
    for (auto r : battle_roles_)
    {
        r->Auto = 1;
    }
}

void BattleScene::actRest(Role* r)
{
    if (!r->Moved)
    {
        r->PhysicalPower = GameUtil::limit(r->PhysicalPower + 5, 0, Role::getMaxValue()->PhysicalPower);
        r->HP = GameUtil::limit(r->HP + 0.05 * r->MaxHP, 0, r->MaxHP);
        r->MP = GameUtil::limit(r->MP + 0.05 * r->MaxMP, 0, r->MaxMP);
    }
    r->Acted = 1;
}

void BattleScene::moveAnimation(Role* r, int x, int y)
{
    //��Ŀ��������ȷ��·��
    std::vector<Point> way;
    auto check_next = [&](Point p1, int step) -> bool
    {
        if (canSelect(p1.x, p1.y) && select_layer_->data(p1.x, p1.y) == step)
        {
            way.push_back(p1);
            return true;
        }
        return false;
    };

    way.push_back({ x, y });
    for (int i = select_layer_->data(x, y); i < select_layer_->data(r->X(), r->Y()); i++)
    {
        int x1 = way.back().x, y1 = way.back().y;
        if (check_next({ x1 - 1, y1 }, i + 1))
        {
            continue;
        }
        if (check_next({ x1 + 1, y1 }, i + 1))
        {
            continue;
        }
        if (check_next({ x1, y1 - 1 }, i + 1))
        {
            continue;
        }
        if (check_next({ x1, y1 + 1 }, i + 1))
        {
            continue;
        }
    }

    for (int i = way.size() - 2; i >= 0; i--)
    {
        if (exit_)
        {
            break;
        }
        r->FaceTowards = calTowards(r->X(), r->Y(), way[i].x, way[i].y);
        r->setPosition(way[i].x, way[i].y);
        //setPosition(r->X(), r->Y());
        drawAndPresent(2);
    }
    r->setPosition(x, y);
    r->Moved = 1;
    select_layer_->setAll(-1);
}

//ʹ����ѧ����
void BattleScene::useMagicAnimation(Role* r, Magic* m)
{
    if (r && m)
    {
        Audio::getInstance()->playASound(m->SoundID);    //���ﲥ����Ч�ϸ�˵����ȷ��������
        actionAnimation(r, m->MagicType, m->EffectID, r->Attack / 20);
    }
}

void BattleScene::actionAnimation(Role* r, int style, int effect_id, int shake /*= 0*/)
{
    if (r->X() != select_x_ || r->Y() != select_y_)
    {
        r->FaceTowards = calTowards(r->X(), r->Y(), select_x_, select_y_);
    }
    if (r->isAuto())
    {
        //�Զ�����r������һ�����ˣ������ź����
        setFaceTowardsNearest(r, true);
    }
    auto frame_count = r->FightFrame[style];
    action_type_ = style;
    //Audio::getInstance()->playASound(style);
    for (action_frame_ = 0; action_frame_ < frame_count; action_frame_++)
    {
        // �������Ч������̧��1֡������
        if (action_frame_ == 1)
        {
            if (r->Effect != -1 || !r->ShowStrings.empty())
            {
                showNumberAnimation(2, false);
                action_frame_ = 0;
                continue;
            }
        }
        if (exit_)
        {
            break;
        }
        drawAndPresent(animation_delay_);
    }
    action_frame_ = frame_count - 1;
    effect_id_ = effect_id;
    auto path = convert::formatString("eft/eft%03d", effect_id_);
    auto effect_count = TextureManager::getInstance()->getTextureGroupCount(path);
    Audio::getInstance()->playESound(effect_id);

    //�ɽ���Զ�Ķ���Ч��
    int min_dis = 9999;
    int max_dis = 0;
    for (int ix = 0; ix < COORD_COUNT; ix++)
    {
        for (int iy = 0; iy < COORD_COUNT; iy++)
        {
            if (haveEffect(ix, iy))
            {
                int dis = calDistance(r->X(), r->Y(), ix, iy);
                min_dis = (std::min)(min_dis, dis);
                max_dis = (std::max)(max_dis, dis);
            }
        }
    }

    for (effect_frame_ = -min_dis; effect_frame_ < effect_count + max_dis + 1; effect_frame_++)
    {
        if (exit_)
        {
            break;
        }
        if (shake > 0)
        {
            x_ = rand_.rand_int(shake) - rand_.rand_int(shake);
            y_ = rand_.rand_int(shake) - rand_.rand_int(shake);
        }
        drawAndPresent(animation_delay_);
    }
    action_frame_ = 0;
    action_type_ = -1;
    effect_frame_ = 0;
    effect_id_ = -1;
    x_ = 0;
    y_ = 0;
}

//r1ʹ���书magic����r2���˺������Ϊһ����
int BattleScene::calMagicHurt(Role* r1, Role* r2, Magic* magic)
{
    int level_index = Save::getInstance()->getRoleLearnedMagicLevelIndex(r1, magic);
    level_index = magic->calMaxLevelIndexByMP(r1->MP, level_index);
    if (magic->HurtType == 0)
    {
        if (r1->MP <= 10)
        {
            return 1 + rand_.rand_int(10);
        }
        int attack = r1->Attack + magic->Attack[level_index] / 3;
        int defence = r2->Defence;

        //װ����Ч��
        if (r1->Equip0 >= 0)
        {
            auto i = Save::getInstance()->getItem(r1->Equip0);
            attack += i->AddAttack;
        }
        if (r1->Equip1 >= 0)
        {
            auto i = Save::getInstance()->getItem(r1->Equip1);
            attack += i->AddAttack;
        }
        if (r2->Equip0 >= 0)
        {
            auto i = Save::getInstance()->getItem(r2->Equip0);
            defence += i->AddDefence;
        }
        if (r2->Equip1 >= 0)
        {
            auto i = Save::getInstance()->getItem(r2->Equip1);
            defence += i->AddDefence;
        }

        int v = attack - defence;
        //����˥��
        int dis = calRoleDistance(r1, r2);
        v = v / exp((dis - 1) / 10);

        v += rand_.rand_int(10) - rand_.rand_int(10);
        if (v < 10)
        {
            v = 1 + rand_.rand_int(10);
        }
        //v = 999;  //������
        return v;
    }
    else if (magic->HurtType == 1)
    {
        int v = magic->HurtMP[level_index];
        v += rand_.rand_int(10) - rand_.rand_int(10);
        if (v < 10)
        {
            v = 1 + rand_.rand_int(10);
        }
        return v;
    }
    return 0;
}

//����ȫ��������˺�
int BattleScene::calMagiclHurtAllEnemies(Role* r, Magic* m, bool simulation)
{
    int total = 0;
    for (auto r2 : battle_roles_)
    {
        //���ҷ��ұ����У�������λ�õ�Ч����Ǹ���
        if (r2->Team != r->Team && haveEffect(r2->X(), r2->Y()))
        {
            int hurt = calMagicHurt(r, r2, m);
            if (!simulation)
            {
                if (m->HurtType == 0)
                {
                    r2->addShowString(convert::formatString("-%d", hurt), { 255, 20, 20, 255 });
                    int prevHP = r2->HP;
                    r2->HP = GameUtil::limit(r2->HP - hurt, 0, r2->MaxHP);
                    int hurt1 = prevHP - r2->HP;
                    r->ExpGot += hurt1;
                    if (r2->HP <= 0)
                    {
                        r->ExpGot += hurt1 / 2;
                    }
                    r2->ProgressChange = -hurt / 5;
                }
                else if (m->HurtType == 1)
                {
                    r2->addShowString(convert::formatString("-%d", hurt), { 160, 32, 240, 255 });
                    int prevMP = r2->MP;
                    r2->MP = GameUtil::limit(r2->MP - hurt, 0, r2->MaxMP);
                    r->MP = GameUtil::limit(r->MP + hurt, 0, r->MaxMP);
                    int hurt1 = prevMP - r2->MP;
                    r->ExpGot += hurt1 / 2;
                }
            }
            else
            {
                //�����Ǽ���ai����
                if (r->AttackTwice)
                {
                    hurt *= 2;
                }
                if (m->HurtType == 0)
                {
                    if (hurt >= r2->HP)
                    {
                        hurt = 1.25 * r2->HP;
                    }
                }
                else if (m->HurtType == 1)
                {
                    hurt /= 5;
                }
            }
            total += hurt;
        }
    }
    return total;
}

//����ֵΪһ����
int BattleScene::calHiddenWeaponHurt(Role* r1, Role* r2, Item* item)
{
    int v = r1->HiddenWeapon - item->AddHP;
    int dis = calRoleDistance(r1, r2);
    v = v / exp((dis - 1) / 10);
    v += rand_.rand_int(10) - rand_.rand_int(10);
    if (v < 1)
    {
        v = 1;
    }
    return v;
}

void BattleScene::showMagicName(std::string name)
{
    auto magic_name = new TextBox();
    magic_name->setText(name);
    magic_name->setPosition(450, 150);
    magic_name->setFontSize(20);
    magic_name->setStayFrame(40);
    magic_name->run();
    delete magic_name;
}

//��ʾ���֣�ͬʱ��ʾ���˽�����
// delay: �ӳ٣�Ĭ��2
// floating: �����Ƿ�Ư��
void BattleScene::showNumberAnimation(int delay, bool floating)
{
    //�ж��Ƿ�����Ҫ��ʾ������
    bool need_show = false;
    int total_frames = 15;
    // ����id -> (eft·��, eft����)
    // std::unordered_map<int, std::pair<std::string, int>> efts;
    for (auto r : battle_roles_)
    {
        if (!r->ShowStrings.empty())
        {
            need_show = true;
        }
        if (r->Effect != -1)
        {
            need_show = true;
            auto path = convert::formatString("eft/eft%03d", r->Effect);
            auto effect_count = TextureManager::getInstance()->getTextureGroupCount(path);
            total_frames = (std::max)(total_frames, effect_count);
            // efts[r->ID] = { path, effect_count };
        }
    }
    if (!need_show)
    {
        return;
    }

    for (int i_frame = 0; i_frame < total_frames; i_frame++)
    {
        if (exit_)
        {
            break;
        }
        auto drawNumber = [&](void*) -> void
        {
            for (auto r : battle_roles_)
            {
                auto p = getPositionOnWindow(r->X(), r->Y(), man_x_, man_y_);
                // ��Խ�籣����ֱ����ʾ�ͺ���
                if (r->Effect != -1)
                {
                    auto path = convert::formatString("eft/eft%03d", r->Effect);
                    TextureManager::getInstance()->renderTexture(path, i_frame, p.x, p.y, { 255, 255, 255, 255 }, 224);
                }
                if (!r->ShowStrings.empty())
                {
                    int y_pos = -75;
                    for (int i_show = 0; i_show < r->ShowStrings.size(); i_show++)
                    {
                        auto& show_string = r->ShowStrings[i_show];
                        int x = p.x - show_string.Size * show_string.Text.size() / 4;
                        int y = p.y - i_frame * 2 + y_pos;
                        if (!floating)
                        {
                            // ����һ��
                            y = p.y - total_frames + y_pos;
                        }
                        Font::getInstance()->draw(show_string.Text, show_string.Size, x, y, show_string.Color, 255 - 255 / total_frames * i_frame);
                        y_pos += show_string.Size + 2;
                    }
                }
                r->Progress += r->ProgressChange / total_frames;
            }
        };
        drawAndPresent(delay, drawNumber);
    }
    //��������˵���ʾ�������ܱ������Ļ���ֵ
    for (auto r : battle_roles_)
    {
        r->Effect = -1;
        r->clearShowStrings();
        r->Progress += r->ProgressChange - total_frames * (r->ProgressChange / total_frames);
        r->ProgressChange = 0;
    }
}

void BattleScene::clearDead()
{
    //�ж��Ƿ�����Ӧ�˳�
    bool found_dead = false;
    for (auto r : battle_roles_)
    {
        if (r->HP <= 0)
        {
            found_dead = true;
            break;
        }
    }
    if (!found_dead)
    {
        return;
    }

    //�˳���������������
    for (int i = 0; i <= 25; i++)
    {
        dead_alpha_ = 255 - i * 10;
        if (dead_alpha_ < 0)
        {
            dead_alpha_ = 0;
        }
        drawAndPresent(animation_delay_);
    }
    dead_alpha_ = 255;

    std::vector<Role*> alive;
    for (auto r : battle_roles_)
    {
        if (r->HP > 0)
        {
            alive.push_back(r);
        }
        else
        {
            r->setPosition(-1, -1);
        }
    }
    battle_roles_ = alive;
}

//�ж���Ч��
void BattleScene::poisonEffect(Role* r)
{
    if (r)
    {
        //�������߻��Զ��ⶾ
        r->Poison -= r->AntiPoison;
        GameUtil::limit2(r->Poison, 0, Role::getMaxValue()->Poison);
        r->HP -= r->Poison / 3;
        //��Ϳ۵�1��
        if (r->HP < 1)
        {
            r->HP = 1;
        }
    }
}

int BattleScene::getTeamMateCount(int team)
{
    int count = 0;
    for (auto r : battle_roles_)
    {
        if (r->Team == team)
        {
            count++;
        }
    }
    return count;
}

//����Ƿ���һ��ȫ��
//���ظ�ֵ��ʾ������������طǸ���Ϊʤ������team���
//ʵ����ֻ�Ǽ���ҷ������뵱ǰ�������Ƿ���Ȼ���Ϊ0
//�����ӵ��ж���ʹ��set����map
//0-�ҷ�ʤ��1-�з�ʤ��-1-ʤ��δ��
int BattleScene::checkResult()
{
    int team0 = getTeamMateCount(0);
    if (team0 == battle_roles_.size())
    {
        return 0;
    }
    if (team0 == 0)
    {
        return 1;
    }
    return -1;
}

void BattleScene::calExpGot()
{
    head_self_->setVisible(false);

    std::vector<Role*> alive_teammate;
    if (result_ == 0)
    {
        for (auto r : battle_roles_)
        {
            if (r->Team == 0)
            {
                alive_teammate.push_back(r);
            }
        }
    }
    else
    {
        alive_teammate = friends_;
    }
    if (alive_teammate.empty())
    {
        return;
    }
    //���ڳ����˻�þ���
    for (auto r : alive_teammate)
    {
        r->ExpGot += info_->Exp / alive_teammate.size();
    }

    auto show_exp = new ShowExp();
    show_exp->setRoles(alive_teammate);
    show_exp->run();
    delete show_exp;

    //������������Ʒ
    auto diff = new ShowRoleDifference();
    for (auto r : alive_teammate)
    {
        if (exit_)
        {
            break;
        }

        Role r0 = *r;    //���ڱȽϵ�״̬

        auto item = Save::getInstance()->getItem(r->PracticeItem);

        if (r->Level >= Role::getMaxValue()->Level)
        {
            //��������ȫ�ӵ���Ʒ����
            r->ExpForItem += r->ExpGot;
        }
        else if (item)
        {
            //δ������ƽ�־���
            r->Exp += r->ExpGot / 2;
            r->ExpForItem += r->ExpGot / 2;
        }
        else
        {
            //�������ȫ�ӵ����ﾭ��
            r->Exp += r->ExpGot;
        }
        r->ExpForMakeItem += r->ExpGot;

        //����Խ��
        if (r->Exp < r0.Exp)
        {
            r->Exp = Role::getMaxValue()->Exp;
        }
        if (r->ExpForItem < r0.ExpForItem)
        {
            r->ExpForItem = Role::getMaxValue()->Exp;
        }

        //����
        int change = 0;
        while (GameUtil::canLevelUp(r))
        {
            GameUtil::levelUp(r);
            change++;
        }
        if (change)
        {
            diff->setTwinRole(&r0, r);
            diff->setText("����");
            diff->run();
        }

        //��������
        if (item)
        {
            r0 = *r;
            change = 0;
            while (GameUtil::canFinishedItem(r))
            {
                GameUtil::useItem(r, item);
                change++;
            }
            if (change)
            {
                diff->setTwinRole(&r0, r);
                diff->setText(convert::formatString("�ޟ�%s�ɹ�", item->Name));
                diff->run();
            }
            if (item->MakeItem[0] >= 0 && r->ExpForMakeItem >= item->NeedExpForMakeItem && Event::getInstance()->haveItemBool(item->NeedMaterial))
            {
                std::vector<ItemList> make_item;
                for (int i = 0; i < 5; i++)
                {
                    if (item->MakeItem[i] >= 0)
                    {
                        make_item.push_back({ item->MakeItem[i], item->MakeItemCount[i] });
                    }
                }
                int index = rand_.rand_int(make_item.size());
                Event::getInstance()->addItem(make_item[index].item_id, make_item[index].count);
                Event::getInstance()->addItemWithoutHint(item->NeedMaterial, -1);
                r->ExpForMakeItem = 0;
            }
        }
    }
    delete diff;
}
