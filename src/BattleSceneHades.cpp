#include "BattleSceneHades.h"
#include "Audio.h"
#include "Font.h"
#include "GameUtil.h"
#include "Head.h"
#include "MainScene.h"

#define M_PI 3.1415926535897

BattleSceneHades::BattleSceneHades()
{
    full_window_ = 1;
    COORD_COUNT = BATTLEMAP_COORD_COUNT;

    earth_layer_.resize(COORD_COUNT);
    building_layer_.resize(COORD_COUNT);

    head_self_ = std::make_shared<Head>();
    addChild(head_self_);

    menu_ = std::make_shared<Menu>();
    menu_->setPosition(50, 200);
    equip_magics_.resize(4);
    for (auto& em : equip_magics_)
    {
        em = std::make_shared<Button>();
    }
    equip_magics_[0]->setText("__________");
    menu_->addChild(equip_magics_[0], 0, 10);
    equip_magics_[1]->setText("__________");
    menu_->addChild(equip_magics_[1], 120, 0);
    equip_magics_[2]->setText("__________");
    menu_->addChild(equip_magics_[2], 240, 10);
    equip_magics_[3]->setText("__________");
    menu_->addChild(equip_magics_[3], 120, 25);
    addChild(menu_);

}

BattleSceneHades::BattleSceneHades(int id) : BattleSceneHades()
{
    setID(id);
}

BattleSceneHades::~BattleSceneHades()
{
}

void BattleSceneHades::setID(int id)
{
    battle_id_ = id;
    info_ = BattleMap::getInstance()->getBattleInfo(id);

    BattleMap::getInstance()->copyLayerData(info_->BattleFieldID, 0, &earth_layer_);
    BattleMap::getInstance()->copyLayerData(info_->BattleFieldID, 1, &building_layer_);
}

void BattleSceneHades::draw()
{
    //在这个模式下，使用的是直角坐标
    Engine::getInstance()->setRenderAssistTexture();
    Engine::getInstance()->fillColor({ 0, 0, 0, 255 }, 0, 0, render_center_x_ * 2, render_center_y_ * 2);

    //以下是计算出需要画的区域，先画到一个大图上，再转贴到窗口
    {
        auto p = pos90To45(man_x1_, man_y1_);
        man_x_ = p.x;
        man_y_ = p.y;
    }
    //一整块地面
    if (earth_texture_)
    {
        Engine::getInstance()->setRenderTarget(earth_texture_);
        Engine::getInstance()->fillColor({ 0, 0, 0, 255 }, 0, 0, COORD_COUNT * TILE_W * 2, COORD_COUNT * TILE_H * 2);
        for (int sum = -view_sum_region_; sum <= view_sum_region_ + 15; sum++)
        {
            for (int i = -view_width_region_; i <= view_width_region_; i++)
            {
                int ix = man_x_ + i + (sum / 2);
                int iy = man_y_ - i + (sum - sum / 2);
                auto p = pos45To90(ix, iy);
                if (!isOutLine(ix, iy))
                {
                    int num = earth_layer_.data(ix, iy) / 2;
                    BP_Color color = { 255, 255, 255, 255 };
                    bool need_draw = true;
                    if (need_draw && num > 0)
                    {
                        TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y / 2, color);
                    }
                }
            }
        }

        struct DrawInfo
        {
            std::string path;
            int num;
            Point p;
            BP_Color color{ 255, 255, 255, 255 };
            uint8_t alpha;
            int rot = 0;
        };
        std::vector<DrawInfo> building_vec;
        building_vec.reserve(10000);

        for (int sum = -view_sum_region_; sum <= view_sum_region_ + 15; sum++)
        {
            for (int i = -view_width_region_; i <= view_width_region_; i++)
            {

                int ix = man_x_ + i + (sum / 2);
                int iy = man_y_ - i + (sum - sum / 2);
                auto p = pos45To90(ix, iy);
                if (!isOutLine(ix, iy))
                {
                    int num = building_layer_.data(ix, iy) / 2;
                    if (num > 0)
                    {
                        //TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y/2);
                        building_vec.emplace_back("smap", num, p);
                    }
                }
            }
        }

        for (auto r : battle_roles_)
        {
            //if (r->Dead) { continue; }
            DrawInfo info;
            info.path = fmt1::format("fight/fight{:03}", r->HeadID);
            info.color = { 255, 255, 255, 255 };
            info.alpha = 255;
            if (battle_cursor_->isRunning() && !acting_role_->isAuto())
            {
                info.color = { 128, 128, 128, 255 };
                if (inEffect(acting_role_, r))
                {
                    info.color = { 255, 255, 255, 255 };
                }
            }
            //int pic;
            //if (r == acting_role_)
            //{
            //    pic = calRolePic(r, action_type_, action_frame_);
            //}
            //else
            //{
            //    pic = calRolePic(r);
            //}
            //if (r->HP <= 0)
            //{
            //    alpha = dead_alpha_;
            //}
            info.p.x = r->X1;
            info.p.y = r->Y1;
            info.num = calRolePic(r, r->ActType, r->ActFrame);
            if (r->HurtFrame)
            {
                info.color = { 255, 0, 0, 255 };
            }
            if (r->Dead)
            {
                if (r->FaceTowards >= 2)
                {
                    info.rot = 90;
                }
                else
                {
                    info.rot = 270;
                }
            }
            //TextureManager::getInstance()->renderTexture(path, pic, r->X1, r->Y1, color, alpha);
            building_vec.emplace_back(std::move(info));
        }

        //effects
        for (auto& ae : attack_effects_)
        {
            //for (auto r : ae.Defender)
            {
                DrawInfo info;
                info.path = ae.Path;
                info.num = ae.Frame % ae.TotalEffectFrame;
                info.p.x = ae.X1;
                info.p.y = ae.Y1;
                info.color = { 255, 255, 255, 255 };
                info.alpha = 192;
                building_vec.emplace_back(std::move(info));
                //TextureManager::getInstance()->renderTexture(ae.Path, ae.Frame % ae.TotalEffectFrame, ae.X1, ae.Y1 / 2, { 255, 255, 255, 255 }, 192);
            }
        }

        auto sort_building = [](DrawInfo& d1, DrawInfo& d2)
        {
            if (d1.p.y != d2.p.y)
            {
                return d1.p.y < d2.p.y;
            }
            else
            {
                return d1.p.x < d2.p.x;
            }
        };
        std::sort(building_vec.begin(), building_vec.end(), sort_building);
        for (auto& d : building_vec)
        {
            double scaley = 1;
            if (d.rot)
            {
                scaley = 0.5;
            }
            TextureManager::getInstance()->renderTexture(d.path, d.num, d.p.x, d.p.y / 2, d.color, 255, scaley, 1, d.rot);
        }

        for (auto r : battle_roles_)
        {
            renderExtraRoleInfo(r, r->X1, r->Y1 / 2);
        }

        BP_Color c = { 255, 255, 255, 255 };
        Engine::getInstance()->setColor(earth_texture_, c);
        int w = render_center_x_ * 2;
        int h = render_center_y_ * 2;
        //获取的是中心位置，如贴图应减掉屏幕尺寸的一半
        BP_Rect rect0 = { int(man_x1_ - render_center_x_ - x_), int(man_y1_ / 2 - render_center_y_ - y_), w, h }, rect1 = { 0, 0, w, h };

        for (auto& te : text_effects_)
        {
            Font::getInstance()->draw(te.Text, 15, te.X1, te.Y1 / 2, te.Color, 255);
        }

        Engine::getInstance()->setRenderAssistTexture();
        Engine::getInstance()->renderCopy(earth_texture_, &rect0, &rect1, 1);


    }

    Engine::getInstance()->renderAssistTextureToWindow();

    if (result_ >= 0)
    {
        Engine::getInstance()->fillColor({ 0, 0, 0, 128 }, 0, 0, -1, -1);
    }
}

void BattleSceneHades::dealEvent(BP_Event& e)
{
    auto engine = Engine::getInstance();
    auto r = role_;

    //方向

    man_x1_ = r->X1;
    man_y1_ = r->Y1;
    if (r->CoolDown == 0)
    {
        //if (current_frame_ % 3 == 0)
        {
            if (engine->checkKeyPress(BPK_a))
            {
                man_x1_ -= r->Speed / 30.0;
                r->FaceTowards = Towards_LeftDown;
                r->ActType2 = 0;
            }
            if (engine->checkKeyPress(BPK_d))
            {
                man_x1_ += r->Speed / 30.0;
                r->FaceTowards = Towards_RightUp;
                r->ActType2 = 0;
            }
            if (engine->checkKeyPress(BPK_w))
            {
                man_y1_ -= r->Speed / 30.0;
                r->FaceTowards = Towards_LeftUp;
                r->ActType2 = 0;
            }
            if (engine->checkKeyPress(BPK_s))
            {
                man_y1_ += r->Speed / 30.0;
                r->FaceTowards = Towards_RightDown;
                r->ActType2 = 0;
            }
        }
        if (engine->checkKeyPress(BPK_1))
        {
            weapon_ = 1;
        }
        if (engine->checkKeyPress(BPK_2))
        {
            weapon_ = 2;
        }
        if (engine->checkKeyPress(BPK_3))
        {
            weapon_ = 3;
        }
        if (engine->checkKeyPress(BPK_4))
        {
            weapon_ = 4;
        }
    }
    if (engine->checkKeyPress(BPK_w) && engine->checkKeyPress(BPK_d))
    {
        r->FaceTowards = Towards_RightUp;
    }
    if (engine->checkKeyPress(BPK_s) && engine->checkKeyPress(BPK_a))
    {
        r->FaceTowards = Towards_LeftDown;
    }
    //实际的朝向可以不能走到
    if (man_x1_ != r->X1 || man_y1_ != r->Y1)
    {
        r->TowardsX1 = man_x1_ - r->X1;
        r->TowardsY1 = man_y1_ - r->Y1;
    }

    if (canWalk90(man_x1_, man_y1_, r))
    {
        r->X1 = man_x1_;
        r->Y1 = man_y1_;
    }

    std::vector<Magic*> magic(4);
    for (int i = 0; i < 4; i++)
    {
        magic[i] = Save::getInstance()->getMagic(r->EquipMagic[i]);
        equip_magics_[i]->setState(Normal);
    }
    if (r->CoolDown == 0)
    {
        int index = -1;
        if (engine->checkKeyPress(BPK_j)) { index = 0; }
        if (engine->checkKeyPress(BPK_i)) { index = 1; }
        if (engine->checkKeyPress(BPK_k)) { index = 2; }
        if (engine->checkKeyPress(BPK_m)) { index = 3; }
        r->ActType2 = index;
        if (index >= 0 && magic[index])
        {
            equip_magics_[index]->setState(Pass);
            auto m = magic[index];
            r->ActType = m->MagicType;
            r->UsingMagic = m;
            r->ActFrame = 0;
            if (index == 0 && r->PhysicalPower >= 10)
            {
                //轻击
                r->CoolDown = 10;
            }
            if (index == 1 && r->PhysicalPower >= 30)
            {
                //蓄力击
                r->CoolDown = 10;
            }
            if (index == 2 && r->PhysicalPower >= 20)
            {
                //远程
                r->CoolDown = 10;
            }
            if (index == 3 && r->PhysicalPower >= 10)
            {
                //闪身                    
                r->CoolDown = 10;    //冷却更长，有收招硬直
                r->SpeedX1 = r->TowardsX1;
                r->SpeedY1 = r->TowardsY1;
                norm(r->SpeedX1, r->SpeedY1, 10);
                r->SpeedFrame = 10;
                r->PhysicalPower -= 2;
                r->ActType = 0;
            }
        }
    }
    backRun1();
}

void BattleSceneHades::dealEvent2(BP_Event& e)
{

}

void BattleSceneHades::backRun1()
{
    for (auto r : battle_roles_)
    {
        if (r->SpeedFrame > 0)
        {
            auto x = r->X1 + r->SpeedX1;
            auto y = r->Y1 + r->SpeedY1;
            if (canWalk90(x, y, r, TILE_W / 2))
            {
                r->X1 = x;
                r->Y1 = y;
            }
            r->SpeedFrame--;
            //r->FaceTowards = rand_.rand() * 4;
        }
        else
        {
            r->SpeedX1 = 0;
            r->SpeedY1 = 0;
            if (r->HP <= 0)
            {
                r->Dead = 1;
            }
        }
        if (r->CoolDown > 0) { r->CoolDown--; }
        if (r->CoolDown == 0)
        {
            if (current_frame_ % 3 == 0)
            {
                r->PhysicalPower += 1;
                r->MP += 1;
            }
            r->PhysicalPower = GameUtil::limit(r->PhysicalPower, 0, 100);
            r->MP = GameUtil::limit(r->MP, 0, r->MaxMP);
            r->Acted = 0;
        }
        if (r->HurtFrame > 0) { r->HurtFrame--; }
    }

    //if (current_frame_ % 2 == 0)
    {
        int current_frame2 = current_frame_;
        for (auto r : battle_roles_)
        {
            //帧数，动画，音效等
            if (r->ActType >= 0)
            {
                //音效和动画
                if (r->Acted == 0
                    && r->ActType2 >= 0
                    //&& r->ActFrame == r->FightFrame[r->ActType] - 3
                    && r->ActFrame == 2)
                {
                    r->Acted = 1;
                    Magic* magic = nullptr;
                    if (r->UsingMagic)
                    {
                        magic = r->UsingMagic;
                    }
                    else
                    {
                        std::vector<Magic*> v;
                        for (int i = 0; i < ROLE_MAGIC_COUNT; i++)
                        {
                            if (r->MagicID[i] > 0)
                            {
                                auto m = Save::getInstance()->getMagic(r->MagicID[i]);
                                if (m->MagicType == r->ActType)
                                {
                                    v.push_back(m);
                                }
                            }
                        }
                        if (!v.empty())
                        {
                            magic = v[rand_.rand() * v.size()];
                        }
                    }
                    AttackEffect ae;
                    if (magic)
                    {
                        Audio::getInstance()->playESound(magic->SoundID);
                        ae.Path = fmt1::format("eft/eft{:03}", magic->EffectID);
                        ae.UsingMagic = magic;
                    }
                    else
                    {
                        Audio::getInstance()->playESound(r->ActType);
                        ae.Path = "eft/eft001";
                        magic = Save::getInstance()->getMagic(1);
                        ae.UsingMagic = Save::getInstance()->getMagic(1);
                    }
                    r->PhysicalPower = GameUtil::limit(r->PhysicalPower - 3, 0, Role::getMaxValue()->PhysicalPower);
                    int level_index = r->getMagicLevelIndex(magic->ID);
                    int needMP = magic->calNeedMP(level_index);
                    ae.TotalEffectFrame = TextureManager::getInstance()->getTextureGroupCount(ae.Path);
                    ae.TotalFrame = 30;
                    //r->CoolDown += ae.TotalFrame;
                    ae.Attacker = r;
                    norm(r->TowardsX1, r->TowardsY1);
                    ae.X1 = r->X1 + TILE_W * 2 * r->TowardsX1;
                    ae.Y1 = r->Y1 + TILE_W * 2 * r->TowardsY1;
                    ae.Frame = 0;
                    ae.ActType2 = r->ActType2;
                    if (r->Team == 0)
                    {
                        if (ae.ActType2 == 0)
                        {
                            ae.TotalFrame = 10;
                            attack_effects_.push_back(std::move(ae));
                            needMP /= 5;
                        }
                        else if (ae.ActType2 == 1)
                        {
                            int range = magic->AttackDistance[level_index] + magic->SelectDistance[level_index];
                            int count = range;
                            int x1 = ae.X1;
                            int y1 = ae.Y1;
                            ae.TotalFrame = 60;
                            for (int i = 0; i < count; i++)
                            {
                                ae.X1 = x1;
                                ae.Y1 = y1;
                                ae.SpeedX1 = cos(i * 2 * M_PI / count);
                                ae.SpeedY1 = sin(i * 2 * M_PI / count);
                                norm(ae.SpeedX1, ae.SpeedY1, 3);
                                attack_effects_.push_back(ae);
                            }
                        }
                        else if (ae.ActType2 == 2)
                        {
                            double dis = 4096;
                            Role* r0 = nullptr;
                            for (auto r1 : battle_roles_)
                            {
                                if (r1->Dead == 0 && r->Team != r1->Team)
                                {
                                    auto dis1 = EuclidDis(r->X1 - r1->X1, r->Y1 - r1->Y1);
                                    if (dis1 < dis)
                                    {
                                        dis = dis1;
                                        r0 = r1;
                                    }
                                }
                            }
                            if (r0)
                            {
                                ae.SpeedX1 = r0->X1 - r->X1;
                                ae.SpeedY1 = r0->Y1 - r->Y1;
                            }
                            else
                            {
                                ae.SpeedX1 = r->TowardsX1;
                                ae.SpeedY1 = r->TowardsY1;
                            }
                            norm(ae.SpeedX1, ae.SpeedY1, 5);
                            ae.TotalFrame = 30;
                            attack_effects_.push_back(std::move(ae));
                            needMP *= 2;
                        }
                        else if (ae.ActType2 == 3)
                        {
                            int x1 = ae.X1;
                            int y1 = ae.Y1;
                            for (int i = 0; i < 3; i++)
                            {
                                ae.X1 = x1 + r->SpeedX1 * (i - 1) * 2;
                                ae.Y1 = y1 + r->SpeedY1 * (i - 1) * 2;
                                ae.Frame += 3;
                                attack_effects_.push_back(ae);
                            }
                            needMP /= 2;
                        }
                    }
                    else
                    {
                        attack_effects_.push_back(ae);
                        int range = magic->AttackDistance[level_index] + magic->SelectDistance[level_index];
                        int count = range;
                        int x1 = ae.X1;
                        int y1 = ae.Y1;
                        for (int i = 0; i < count; i++)
                        {
                            ae.X1 = x1 + (rand_.rand() * 2 - 1) * range * TILE_W / 2;
                            ae.Y1 = y1 + (rand_.rand() * 2 - 1) * range * TILE_W / 2;
                            ae.Frame += rand_.rand() * 5;
                            attack_effects_.push_back(ae);
                        }
                    }
                    r->MP = GameUtil::limit(r->MP - needMP, 0, r->MaxMP);
                }
                //动作帧数计算
                if (r->ActFrame >= r->FightFrame[r->ActType] - 2)
                {
                    if (r->CoolDown == 0)
                    {
                        r->ActType = -1;
                        r->ActFrame = 0;
                    }
                    else
                    {
                        r->ActFrame = r->FightFrame[r->ActType] - 2;    //cooldown结束前停在最后行动帧，最后一帧一般是无行动的图
                    }
                }
                else
                {
                    if (r->ActType2 == 1)
                    {
                        //if (current_frame2 % 4 == 0)
                        {
                            r->ActFrame++;
                            if (r->ActFrame >= 7)
                            {
                                x_ = rand_.rand_int(2) - rand_.rand_int(2);
                                y_ = rand_.rand_int(2) - rand_.rand_int(2);
                            }
                        }
                    }
                    else
                    {
                        r->ActFrame++;
                    }
                }
            }
            else
            {
                r->ActFrame = 0;
            }

            //计算有人被打中
            for (auto& ae : attack_effects_)
            {
                if (!(r->ActType2 == 2 && r->CoolDown)
                    && !r->HurtFrame
                    && r->Dead == 0
                    && r->Team != ae.Attacker->Team
                    && ae.Defender.count(r) == 0
                    && EuclidDis(r->X1 - ae.X1, r->Y1 - ae.Y1) <= TILE_W * 2)
                {
                    ae.Defender[r]++;
                    int hurt = calMagicHurt(ae.Attacker, r, ae.UsingMagic, EuclidDis(r->X1 - ae.Attacker->X1, r->Y1 - ae.Attacker->Y1) / TILE_W / 2);
                    if (ae.ActType2 > 0)
                    {
                        //这个击退好像效果不太对
                        r->SpeedX1 = r->X1 - ae.Attacker->X1;
                        r->SpeedY1 = r->Y1 - ae.Attacker->Y1;
                        norm(r->SpeedX1, r->SpeedY1, 1);
                        r->SpeedFrame = 10;
                        r->HurtFrame = 20;
                        if (ae.ActType2 == 1)
                        {
                            hurt /= 5;
                            ae.Frame = ae.TotalFrame + 1;
                        }
                        if (ae.ActType2 == 2)
                        {
                            hurt /= 3;
                            ae.Frame = ae.TotalFrame + 1;
                        }
                        if (ae.ActType2 == 3)
                        {
                            hurt /= 5;
                        }
                        if (r->MP >= hurt)
                        {
                            r->MP -= hurt;
                        }
                        else
                        {
                            r->CoolDown = 0;
                            r->ActType = -1;
                            r->ActType2 = -1;
                        }
                    }
                    else
                    {
                        hurt /= 5;
                    }
                    if (hurt <= 0)
                    {
                        hurt = 1 + rand_.rand() * 3;
                    }
                    r->HP -= hurt;
                    fmt1::print("{} attack {} with {}, hurt {}\n", ae.Attacker->Name, r->Name, ae.UsingMagic->Name, hurt);
                    TextEffect te;
                    te.Color = { 255,255,255,255 };
                    if (r->Team == 0)
                    {
                        te.Color = { 255,0,0,255 };
                    }
                    te.Text = std::to_string(-hurt);
                    te.X1 = r->X1;
                    te.Y1 = r->Y1 - 50;
                    text_effects_.push_back(std::move(te));
                    if (r->HP <= 0)
                    {
                        r->Dead = 1;
                        r->HP = 0;
                        r->SpeedX1 = r->X1 - ae.Attacker->X1;
                        r->SpeedY1 = r->Y1 - ae.Attacker->Y1;
                        norm(r->SpeedX1, r->SpeedY1, 30);
                        r->SpeedFrame = 10;
                    }
                }
            }
            //ai
            if (r != role_ && r->Dead == 0)
            {
                if (r->CoolDown == 0)
                {
                    double dis = 4096;
                    Role* r0 = nullptr;
                    for (auto r1 : battle_roles_)
                    {
                        if (r1->Dead == 0 && r->Team != r1->Team)
                        {
                            auto dis1 = EuclidDis(r->X1 - r1->X1, r->Y1 - r1->Y1);
                            if (dis1 < dis)
                            {
                                dis = dis1;
                                r0 = r1;
                            }
                        }
                    }
                    if (r0)
                    {
                        if (EuclidDis(r->X1 - r0->X1, r->Y1 - r0->Y1) > TILE_W * 2.5)
                        {
                            r->TowardsX1 = r0->X1 - r->X1;
                            r->TowardsY1 = r0->Y1 - r->Y1;
                            r->FaceTowards = Towards_RightDown;
                            if (r->TowardsX1 > 0 && r->TowardsY1 < 0) { r->FaceTowards = Towards_RightUp; }
                            if (r->TowardsX1 < 0 && r->TowardsY1 > 0) { r->FaceTowards = Towards_LeftDown; }
                            if (r->TowardsX1 < 0 && r->TowardsY1 < 0) { r->FaceTowards = Towards_LeftUp; }
                            norm(r->TowardsX1, r->TowardsY1);
                            auto x = r->X1 + 6 * r->TowardsX1;
                            auto y = r->Y1 + 6 * r->TowardsY1;
                            if (canWalk90(x, y, r, TILE_W))
                            {
                                r->X1 = r->X1 + r->Speed / 20.0 * r->TowardsX1;
                                r->Y1 = r->Y1 + r->Speed / 20.0 * r->TowardsY1;
                            }
                        }
                        else
                        {
                            //attack
                            if (r->PhysicalPower >= 30)
                            {
                                //r->PhysicalPower -= 5;
                                for (int i = 1; i <= 4; i++)
                                {
                                    if (r->FightFrame[i] > 0)
                                    {
                                        r->ActType2 = 1;
                                        r->CoolDown = 10;
                                        r->ActFrame = 0;
                                        r->ActType = i;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    //删除播放完毕的
    for (auto it = attack_effects_.begin(); it != attack_effects_.end();)
    {
        if (it->Frame >= it->TotalFrame)
        {
            it = attack_effects_.erase(it);
        }
        else
        {
            it++;
        }
    }
    //效果
    //if (current_frame_ % 2 == 0)
    {
        for (auto& ae : attack_effects_)
        {
            ae.Frame++;
            ae.X1 += ae.SpeedX1;
            ae.Y1 += ae.SpeedY1;
        }
    }
    for (auto it = text_effects_.begin(); it != text_effects_.end();)
    {
        if (it->Frame >= 30)
        {
            it = text_effects_.erase(it);
        }
        else
        {
            it++;
        }
    }
    for (auto& te : text_effects_)
    {
        te.Y1 -= 2;
        text_effects_[0].Frame++;
    }
    //清理,应增加效果
    auto role_count = battle_roles_.size();
    //for (auto it = battle_roles_.begin(); it != battle_roles_.end();)
    //{
    //    if ((*it)->Dead)
    //    {
    //        it = battle_roles_.erase(it);
    //    }
    //    else
    //    {
    //        it++;
    //    }
    //}
    if (getTeamMateCount(1) < 5)
    {
        if (!enemies_.empty())
        {
            battle_roles_.push_back(enemies_.front());
            enemies_.pop_front();
        }
    }
    //if (role_count != battle_roles_.size())
    {
        //检测战斗结果
        int battle_result = checkResult();

        //我方胜
        if (battle_result >= 0)
        {
            result_ = battle_result;
            if (result_ == 0 || result_ == 1)
            {
                calExpGot();
                setExit(true);
            }
        }
    }
}

void BattleSceneHades::onEntrance()
{
    calViewRegion();
    Audio::getInstance()->playMusic(info_->Music);
    //注意此时才能得到窗口的大小，用来设置头像的位置
    head_self_->setPosition(80, 100);

    addChild(MainScene::getInstance()->getWeather());

    earth_texture_ = Engine::getInstance()->createARGBRenderedTexture(COORD_COUNT * TILE_W * 2, COORD_COUNT * TILE_H * 2);

    // 正常战斗
    //设置全部角色的位置层，避免今后出错
    for (auto r : Save::getInstance()->getRoles())
    {
        r->setRolePositionLayer(&role_layer_);
        r->Team = 2;    //先全部设置成不存在的阵营
        r->Auto = 1;
        r->Dead = 0;
    }

    //首先设置位置和阵营，其他的后面统一处理
    //敌方
    for (int i = 0; i < BATTLE_ENEMY_COUNT; i++)
    {
        auto r = Save::getInstance()->getRole(info_->Enemy[i]);
        if (r)
        {
            enemies_.push_back(r);
            r->setPositionOnly(info_->EnemyX[i], info_->EnemyY[i]);
            r->Team = 1;
            readFightFrame(r);
            r->FaceTowards = rand_.rand_int(4);
            man_x_ = r->X();
            man_y_ = r->Y();
        }
    }

    //初始状态
    for (auto r : enemies_)
    {
        setRoleInitState(r);
    }

    //敌人按能力从低到高，每次出场2人
    std::sort(enemies_.begin(), enemies_.end(), [](Role* l, Role* r) { return l->Level * 10000 + l->MaxHP < r->Level * 10000 + r->MaxHP; });
    for (int i = 0; i < 2; i++)
    {
        if (!enemies_.empty())
        {
            battle_roles_.push_back(enemies_.front());
            enemies_.pop_front();
        }
    }

    //单通
    role_ = Save::getInstance()->getRole(0);
    role_->setPositionOnly(info_->TeamMateX[0], info_->TeamMateY[0]);
    role_->Team = 0;
    battle_roles_.push_back(role_);

    head_self_->setRole(role_);
    setRoleInitState(role_);
    for (int i = 0; i < 4; i++)
    {
        auto m = Save::getInstance()->getMagic(role_->EquipMagic[i]);
        if (m)
        {
            std::string text = m->Name;
            text += std::string(10 - Font::getTextDrawSize(text), ' ');
            equip_magics_[i]->setText(text);
        }
    }
}

void BattleSceneHades::onExit()
{
    Engine::getInstance()->destroyTexture(earth_texture_);
}

void BattleSceneHades::renderExtraRoleInfo(Role* r, double x, double y)
{
    if (r == nullptr || r->Dead)
    {
        return;
    }
    // 画个血条
    BP_Color outline_color = { 0, 0, 0, 128 };
    BP_Color background_color = { 0, 255, 0, 128 };    // 我方绿色
    if (r->Team == 1)
    {
        // 敌方红色
        background_color = { 255, 0, 0, 128 };
    }
    int hp_max_w = 24;
    int hp_x = x - hp_max_w / 2;
    int hp_y = y - 60;
    int hp_h = 3;
    double perc = ((double)r->HP / r->MaxHP);
    if (perc < 0)
    {
        perc = 0;
    }

    double alpha = 1;
    if (r->HP <= 0)
    {
        alpha = dead_alpha_ / 255.0;
    }
    BP_Rect r0 = { hp_x, hp_y, hp_max_w, hp_h };
    Engine::getInstance()->renderSquareTexture(&r0, outline_color, 128 * alpha);
    BP_Rect r1 = { hp_x, hp_y, int(perc * hp_max_w), hp_h };
    Engine::getInstance()->renderSquareTexture(&r1, background_color, 192 * alpha);
}

int BattleSceneHades::checkResult()
{
    int team0 = getTeamMateCount(0);
    int team1 = enemies_.size() + getTeamMateCount(1);
    if (team0 > 0 && team1 == 0)
    {
        return 0;
    }
    if (team0 == 0 && team1 > 0)
    {
        return 1;
    }
    return -1;
}

void BattleSceneHades::setRoleInitState(Role* r)
{
    BattleScene::setRoleInitState(r);
    if (r->Team == 0)
    {
        r->HP = r->MaxHP;
        r->MP = r->MaxMP;
    }
    else
    {
        r->HP = r->MaxHP = r->MaxHP * 1;
        r->MP = r->MaxMP = r->MaxMP * 1;
    }

    auto p = pos45To90(r->X(), r->Y());

    r->X1 = p.x;
    r->Y1 = p.y;
    if (r->FaceTowards == Towards_RightDown)
    {
        r->TowardsX1 = 1;
        r->TowardsY1 = 1;
    }
    if (r->FaceTowards == Towards_RightUp)
    {
        r->TowardsX1 = 1;
        r->TowardsY1 = -1;
    }
    if (r->FaceTowards == Towards_LeftDown)
    {
        r->TowardsX1 = -1;
        r->TowardsY1 = 1;
    }
    if (r->FaceTowards == Towards_LeftUp)
    {
        r->TowardsX1 = -1;
        r->TowardsY1 = -1;
    }
    r->Progress = 0;
    r->CoolDown = 0;
}

//int BattleSceneHades::calHurt(Role* r0, Role* r1)
//{
//    return 1 + 5 * rand_.rand() + 2 * (pow(r0->Attack, 1.5) / r1->Defence);
//}
