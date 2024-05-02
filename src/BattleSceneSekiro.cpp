#include "BattleSceneSekiro.h"
#include "Audio.h"
#include "MainScene.h"
#include "TeamMenu.h"

BattleSceneSekiro::BattleSceneSekiro()
{
    keys_ = *UIKeyConfig::getKeyConfig();
    full_window_ = 1;
    COORD_COUNT = BATTLEMAP_COORD_COUNT;

    earth_layer_.resize(COORD_COUNT);
    building_layer_.resize(COORD_COUNT);

    heads_.resize(1);
    int i = 0;
    for (auto& h : heads_)
    {
        h = std::make_shared<Head>();
        h->setAlwaysLight(1);
        addChild(h, 10, 10 + (i++) * 80);
        h->setVisible(false);
    }
    heads_[0]->setVisible(true);
    heads_[0]->setRole(Save::getInstance()->getRole(0));

    heads_[0]->setPosition(10, Engine::getInstance()->getWindowHeight() - 100);

    head_boss_.resize(1);
    for (auto& h : head_boss_)
    {
        h = std::make_shared<Head>();
        h->setStyle(2);
        h->setVisible(false);
        addChild(h);
    }
}

void BattleSceneSekiro::setID(int id)
{
    battle_id_ = id;
    info_ = BattleMap::getInstance()->getBattleInfo(id);

    BattleMap::getInstance()->copyLayerData(info_->BattleFieldID, 0, &earth_layer_);
    BattleMap::getInstance()->copyLayerData(info_->BattleFieldID, 1, &building_layer_);
}

void BattleSceneSekiro::draw()
{
    //在这个模式下，使用的是直角坐标
    Engine::getInstance()->setRenderAssistTexture();
    Engine::getInstance()->fillColor({ 0, 0, 0, 255 }, 0, 0, render_center_x_ * 2, render_center_y_ * 2);

    //以下是计算出需要画的区域，先画到一个大图上，再转贴到窗口
    {
        auto p = pos90To45(pos_.x, pos_.y);
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
            Pointf p;
            BP_Color color{ 255, 255, 255, 255 };
            uint8_t alpha = 255;
            int rot = 0;
            int shadow = 0;
            uint8_t white = 0;
        };
        std::vector<DrawInfo> draw_infos;
        draw_infos.reserve(10000);

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
                        DrawInfo info;
                        info.path = "smap";
                        info.num = num;
                        info.p.x = p.x;
                        info.p.y = p.y;
                        info.shadow = 0;
                        draw_infos.emplace_back(std::move(info));
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
            info.white = 0;
            if (battle_cursor_->isRunning() && !acting_role_->isAuto())
            {
                info.color = { 128, 128, 128, 255 };
                if (inEffect(acting_role_, r))
                {
                    info.color = { 255, 255, 255, 255 };
                }
            }
            info.p = r->Pos;
            if (result_ == -1 && r->Shake)
            {
                info.p.x += -2.5 + rand_.rand() * 5;
            }
            r->FaceTowards = realTowardsToFaceTowards(r->RealTowards);
            info.num = calRolePic(r, r->ActType, r->ActFrame);
            //if (r->HurtFrame)
            //{
            //    if (r->HurtFrame % 2 == 1)
            //    {
            //        info.white = 128;
            //    }
            //}
            if (r->Dead)
            {
                //if (r->Frozen == 0)
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
            }
            if (r->Attention)
            {
                info.alpha = 255 - r->Attention * 4;
            }
            info.shadow = 1;
            //TextureManager::getInstance()->renderTexture(path, pic, r->X1, r->Y1, color, alpha);
            draw_infos.emplace_back(std::move(info));
        }

        //effects
        //for (auto& ae : attack_effects_)
        //{
        //    //for (auto r : ae.Defender)
        //    {
        //        DrawInfo info;
        //        info.path = ae.Path;
        //        if (ae.TotalEffectFrame > 0)
        //        {
        //            info.num = ae.Frame % ae.TotalEffectFrame;
        //        }
        //        else
        //        {
        //            info.num = 0;
        //        }
        //        info.p = ae.Pos;
        //        info.color = { 255, 255, 255, 255 };
        //        info.alpha = 192;
        //        if (ae.FollowRole)
        //        {
        //            info.p = ae.FollowRole->Pos;
        //        }
        //        info.shadow = 1;
        //        if (ae.Attacker && ae.Attacker->Team == 0)
        //        {
        //            info.shadow = 2;
        //        }
        //        info.alpha = 255 * (ae.TotalFrame * 1.25 - ae.Frame) / (ae.TotalFrame * 1.25);    //越来越透明
        //        draw_infos.emplace_back(std::move(info));
        //        //TextureManager::getInstance()->renderTexture(ae.Path, ae.Frame % ae.TotalEffectFrame, ae.X1, ae.Y1 / 2, { 255, 255, 255, 255 }, 192);
        //    }
        //}

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
        std::sort(draw_infos.begin(), draw_infos.end(), sort_building);

        //影子
        for (auto& d : draw_infos)
        {
            if (d.shadow)
            {
                auto tex = TextureManager::getInstance()->getTexture(d.path, d.num);
                if (tex)
                {
                    double scalex = 1, scaley = 0.3;
                    int yd = tex->dy * 0.7;
                    if (d.rot)
                    {
                        scalex = 0.3;
                        scaley = 1;
                        yd = tex->dy * 0.1;
                    }
                    if (d.shadow == 1)
                    {
                        TextureManager::getInstance()->renderTexture(tex, d.p.x, d.p.y / 2 + yd, { 32, 32, 32, 255 }, d.alpha / 2, scalex, scaley, d.rot);
                    }
                    if (d.shadow == 2)
                    {
                        TextureManager::getInstance()->renderTexture(tex, d.p.x, d.p.y / 2 + yd, { 128, 128, 128, 255 }, d.alpha / 2, scalex, scaley, d.rot, 128);
                    }
                }
            }
        }
        for (auto& d : draw_infos)
        {
            double scaley = 1;
            if (d.rot)
            {
                scaley = 0.5;
            }
            TextureManager::getInstance()->renderTexture(d.path, d.num, d.p.x, d.p.y / 2 - d.p.z, d.color, d.alpha, scaley, 1, d.rot, d.white);
        }

        for (auto r : battle_roles_)
        {
            renderExtraRoleInfo(r, r->Pos.x, r->Pos.y / 2);
        }

        BP_Color c = { 255, 255, 255, 255 };
        Engine::getInstance()->setColor(earth_texture_, c);
        int w = render_center_x_ * 2;
        int h = render_center_y_ * 2;
        //获取的是中心位置，如贴图应减掉屏幕尺寸的一半
        BP_Rect rect0 = { int(pos_.x - render_center_x_ - x_), int(pos_.y / 2 - render_center_y_ - y_), w, h };
        BP_Rect rect1 = { 0, 0, w, h };
        if (rect0.x < 0)
        {
            rect1.x = -rect0.x;
            rect0.x = 0;
            rect0.w = w - rect1.x;
        }
        if (rect0.y < 0)
        {
            rect1.y = -rect0.y;
            rect0.y = 0;
            rect0.h = h - rect1.y;
        }
        rect0.w = std::min(rect0.w, COORD_COUNT * TILE_W * 2 - rect0.x);
        rect0.h = std::min(rect0.h, COORD_COUNT * TILE_H * 2 - rect0.y);
        rect1.w = rect0.w;
        rect1.h = rect0.h;
        //for (auto& te : text_effects_)
        //{
        //    Font::getInstance()->draw(te.Text, te.Size, te.Pos.x, te.Pos.y / 2, te.Color, 255);
        //}

        Engine::getInstance()->setRenderAssistTexture();
        //if (close_up_)
        //{
        //    rect0.w /= 2;
        //    rect0.h /= 2;
        //    rect0.x += rect0.w / 2;
        //    rect0.y += rect0.h / 2 - 20;
        //}
        Engine::getInstance()->renderCopy(earth_texture_, &rect0, &rect1, 0);
    }

    Engine::getInstance()->renderAssistTextureToWindow();

    //if (result_ >= 0)
    //{
    //    Engine::getInstance()->fillColor({ 0, 0, 0, 128 }, 0, 0, -1, -1);
    //}
}

void BattleSceneSekiro::dealEvent(BP_Event& e)
{
    auto engine = Engine::getInstance();
    auto r = role_;

    pos_ = r->Pos;

    Pointf pos = r->Pos;
    double speed = std::min(4.0, r->Speed / 30.0);
    if (e.type == BP_KEYUP && e.key.keysym.sym == BPK_TAB
        || e.type == BP_CONTROLLERBUTTONUP && e.cbutton.button == BP_CONTROLLER_BUTTON_BACK)
    {
        if (r->Auto == 0) { r->Auto = 1; }
        else { r->Auto = 0; }
    }
    if (e.type == BP_KEYUP && e.key.keysym.sym == BPK_ESCAPE
        || e.type == BP_CONTROLLERBUTTONUP && e.cbutton.button == BP_CONTROLLER_BUTTON_START)
    {
        auto menu2 = std::make_shared<MenuText>();
        menu2->setStrings({ "確認（Y）", "取消（N）" });
        menu2->setPosition(400, 300);
        menu2->setFontSize(24);
        menu2->setHaveBox(true);
        menu2->setText("認輸？");
        menu2->arrange(0, 50, 150, 0);
        if (menu2->run() == 0)
        {
            result_ = 1;
            for (auto r : friends_)
            {
                r->ExpGot = 0;
            }
            setExit(true);
        }
    }
    if (r->Dead == 0)
    {
        if (r->Frozen == 0 && r->CoolDown == 0)
        {
            //if (current_frame_ % 3 == 0)
            {
                auto axis_x = engine->gameControllerGetAxis(BP_CONTROLLER_AXIS_LEFTX);
                auto axis_y = engine->gameControllerGetAxis(BP_CONTROLLER_AXIS_LEFTY);
                if (abs(axis_x) < 6000) { axis_x = 0; }
                if (abs(axis_y) < 6000) { axis_y = 0; }
                if (axis_x != 0 || axis_y != 0)
                {
                    //fmt1::print("{} {}, ", axis_x, axis_y);
                    axis_x = GameUtil::limit(axis_x, -30000, 30000);
                    axis_y = GameUtil::limit(axis_y, -30000, 30000);
                    Pointf axis{ double(axis_x), double(axis_y) };
                    axis *= 1.0 / 30000 / sqrt(2.0);
                    r->RealTowards = axis;
                    //r->FaceTowards = realTowardsToFaceTowards(r->RealTowards);
                    pos += speed * axis;
                }
                Pointf direct;
                if (engine->checkKeyPress(keys_.Left) || engine->checkKeyPress(BPK_LEFT))
                {
                    direct.x = -1;
                    r->FaceTowards = Towards_LeftDown;
                }
                if (engine->checkKeyPress(keys_.Right) || engine->checkKeyPress(BPK_RIGHT))
                {
                    direct.x = 1;
                    r->FaceTowards = Towards_RightUp;
                }
                if (engine->checkKeyPress(keys_.Up) || engine->checkKeyPress(BPK_UP))
                {
                    direct.y = -1;
                    r->FaceTowards = Towards_LeftUp;
                }
                if (engine->checkKeyPress(keys_.Down) || engine->checkKeyPress(BPK_DOWN))
                {
                    direct.y = 1;
                    r->FaceTowards = Towards_RightDown;
                }
                direct.normTo(speed);
                pos += direct;
                //这样来看同时用手柄和键盘会走得很快，就这样吧
            }
        }
        if (engine->checkKeyPress(keys_.Up) && engine->checkKeyPress(keys_.Right)
            || engine->checkKeyPress(BPK_UP) && engine->checkKeyPress(BPK_RIGHT))
        {
            r->FaceTowards = Towards_RightUp;
        }
        if (engine->checkKeyPress(keys_.Down) && engine->checkKeyPress(keys_.Left)
            || engine->checkKeyPress(BPK_DOWN) && engine->checkKeyPress(BPK_LEFT))
        {
            r->FaceTowards = Towards_LeftDown;
        }
        //实际的朝向可以不能走到
        if (pos.x != r->Pos.x || pos.y != r->Pos.y)
        {
            r->RealTowards = pos - r->Pos;
        }

        if (canWalk90(pos, r))
        {
            r->Pos = pos;
        }

        // 初始化武功
        std::vector<Magic*> magic(4);
        for (int i = 0; i < 4; i++)
        {
            magic[i] = Save::getInstance()->getMagic(r->EquipMagic[i]);
            if (magic[i] && r->getMagicOfRoleIndex(magic[i]) < 0) { magic[i] = nullptr; }
            //equip_magics_[i]->setState(NodeNormal);
        }

        if (r->Frozen == 0 && r->CoolDown == 0)
        {
            int index = -1;
            if (r->PhysicalPower >= 10
                && (engine->checkKeyPress(keys_.Light)
                    || engine->gameControllerGetButton(BP_CONTROLLER_BUTTON_X)
                    || (e.type == BP_MOUSEBUTTONDOWN && e.button.button == BP_BUTTON_LEFT)))
            {
                index = 0;
            }
            if (r->PhysicalPower >= 30
                && (engine->checkKeyPress(keys_.Heavy)
                    || engine->gameControllerGetButton(BP_CONTROLLER_BUTTON_Y)
                    || (e.type == BP_MOUSEWHEEL && e.wheel.y > 0)
                    || (e.type == BP_MOUSEBUTTONDOWN && e.button.button == BP_BUTTON_MIDDLE)))
            {
                index = 1;
            }
            if (r->PhysicalPower >= 20
                && (engine->checkKeyPress(keys_.Long)
                    || engine->gameControllerGetButton(BP_CONTROLLER_BUTTON_B)
                    || (e.type == BP_MOUSEBUTTONDOWN && e.button.button == BP_BUTTON_RIGHT)))
            {
                index = 2;
            }
            if (r->PhysicalPower >= 10
                && (engine->checkKeyPress(keys_.Slash)
                    || engine->gameControllerGetButton(BP_CONTROLLER_BUTTON_A)
                    || (e.type == BP_MOUSEWHEEL && e.wheel.y < 0)))
            {
                index = 3;
            }
            if (index >= 0)
            {
                r->Auto = 0;
            }
            if (r->OperationCount >= 3 || current_frame_ - r->PreActTimer > 60)
            {
                r->OperationCount = 0;
            }
            if (index >= 0 && index == r->OperationType)
            {
                r->OperationCount++;
            }

            if (index >= 0 && index < magic.size() && magic[index])
            {
                r->OperationType = index;
                //equip_magics_[index]->setState(NodePass);
                auto m = magic[index];
                r->ActType = m->MagicType;
                r->UsingMagic = m;
                r->UsingItem = nullptr;
                r->ActFrame = 0;
                r->HaveAction = 1;
                //r->Frozen = 5;
                if (index == 0)
                {
                    //点攻
                    //r->CoolDown = 10;
                }
                if (index == 1)
                {
                    //面攻
                    //r->CoolDown = 60;
                }
                if (index == 2)
                {
                    //远程
                    //r->CoolDown = 20;
                }
                if (index == 3)
                {
                    //闪身
                    //r->CoolDown = 10;    //冷却更长，有收招硬直
                }
                //r->CoolDown = calCoolDown(magic[index]->MagicType, index, r);
                if (r->OperationCount >= 3 && index == 0)
                {
                    r->CoolDown *= 2;
                }
            }
        }
    }
    backRun1();
}

void BattleSceneSekiro::dealEvent2(BP_Event& e)
{
}

void BattleSceneSekiro::onEntrance()
{
    calViewRegion();
    Audio::getInstance()->playMusic(info_->Music);
    //注意此时才能得到窗口的大小，用来设置头像的位置
    head_self_->setPosition(10, 10);
    int count = 0;
    for (auto& h : head_boss_)
    {
        h->setPosition(20, 20);
    }
    addChild(MainScene::getInstance()->getWeather());

    earth_texture_ = Engine::getInstance()->createARGBRenderedTexture(COORD_COUNT * TILE_W * 2, COORD_COUNT * TILE_H * 2);

    //首先设置位置和阵营，其他的后面统一处理
    //敌方
    for (int i = 0; i < BATTLE_ENEMY_COUNT; i++)
    {
        auto r_ptr = Save::getInstance()->getRole(info_->Enemy[i]);
        if (r_ptr)
        {
            enemies_obj_.push_back(*r_ptr);
            auto r = &enemies_obj_.back();
            r->resetBattleInfo();
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
    pos_ = enemies_[0]->Pos;

    //敌人按能力从低到高，依次出场
    std::sort(enemies_.begin(), enemies_.end(), [](Role* l, Role* r)
        {
            return l->MaxHP + l->Attack < r->MaxHP + r->Attack;
        });

    for (int i = 0; i < head_boss_.size(); i++)
    {
        bool is_boss = false;
        if (enemies_.size() >= i + 1)
        {
            auto r = enemies_[enemies_.size() - i - 1];
            if (is_boss || r->MaxHP >= 300 || r == enemies_.back())
            {
                is_boss = true;
                head_boss_[i]->setRole(r);
            }
        }
    }
    for (int i = 0; i < 6; i++)
    {
        if (!enemies_.empty())
        {
            battle_roles_.push_back(enemies_.front());
            enemies_.pop_front();
        }
    }

    //判断是不是有自动战斗人物
    if (info_->AutoTeamMate[0] >= 0)
    {
        for (int i = 0; i < TEAMMATE_COUNT; i++)
        {
            auto r = Save::getInstance()->getRole(info_->AutoTeamMate[i]);
            if (r)
            {
                friends_.push_back(r);
                r->Auto = 2;    //由AI控制
            }
        }
    }

    if (1)    //准许队友出场
    {
        auto team_menu = std::make_shared<TeamMenu>();
        team_menu->setMode(1);
        team_menu->setForceMainRole(true);
        team_menu->run();

        for (auto r : team_menu->getRoles())
        {
            if (std::find(friends_.begin(), friends_.end(), r) == friends_.end())
            {
                friends_.push_back(r);
            }
        }
    }
    //队友
    role_ = Save::getInstance()->getRole(0);
    bool have_main = false;
    for (auto r : friends_)
    {
        if (r == role_) { have_main = true; }
    }
    if (!have_main)
    {
        friends_.insert(friends_.begin(), role_);
    }
    for (int i = 0; i < friends_.size(); i++)
    {
        auto r = friends_[i];
        if (r)
        {
            r->resetBattleInfo();
            battle_roles_.push_back(r);
            r->setPositionOnly(info_->TeamMateX[i], info_->TeamMateY[i]);
            r->Team = 0;
            setRoleInitState(r);
        }
    }
    int i = 0;
    for (auto r : friends_)
    {
        if (r && r != heads_[0]->getRole())
        {
            auto head = std::make_shared<Head>();
            head->setRole(r);
            head->setAlwaysLight(true);
            addChild(head, Engine::getInstance()->getWindowWidth() - 280, 10 + 80 * i++);
        }
    }

    //head_self_->setRole(role_);

    //for (int i = 0; i < equip_magics_.size(); i++)
    //{
    //    auto m = Save::getInstance()->getMagic(role_->EquipMagic[i]);
    //    if (m && role_->getMagicOfRoleIndex(m) < 0) { m = nullptr; }
    //    if (m)
    //    {
    //        std::string text = m->Name;
    //        text += std::string(10 - Font::getTextDrawSize(text), ' ');
    //        equip_magics_[i]->setText(text);
    //    }
    //    else
    //    {
    //        equip_magics_[i]->setText("__________");
    //    }
    //}
    //menu_->setVisible(true);
}

void BattleSceneSekiro::onExit()
{
}

void BattleSceneSekiro::backRun1()
{
    {
        int current_frame2 = current_frame_;
        for (auto r : battle_roles_)
        {
            //有行动
            Action(r);
            //ai策略
            AI(r);
        }
    }
    {
        //人物出场
        if (getTeamMateCount(1) < 5)
        {
            if (!enemies_.empty())
            {
                battle_roles_.push_back(enemies_.front());
                enemies_.pop_front();
                battle_roles_.back()->Attention = 30;
                battle_roles_.back()->CoolDown = 30;
                battle_roles_.back()->Invincible = 30;
            }
        }
        //亮血条
        if (enemies_.size() < head_boss_.size())
        {
            for (int i = 0; i < head_boss_.size(); i++)
            {
                if (i >= enemies_.size())
                {
                    head_boss_[i]->setVisible(true);
                }
            }
        }
        //检测战斗结果
        int battle_result = checkResult();

        //if (battle_result >= 0)
        //{
        //    if (result_ == -1)
        //    {
        //        //pos_ = dying_->Pos;
        //        close_up_ = 60;
        //        frozen_ = 60;
        //        slow_ = 40;
        //        shake_ = 60;
        //        result_ = battle_result;
        //    }
        //    if (slow_ == 0 && (result_ == 0 || result_ == 1))
        //    {
        //        menu_->setVisible(false);
        //        calExpGot();
        //        setExit(true);
        //    }
        //}
    }
}

void BattleSceneSekiro::Action(Role* r)
{}

void BattleSceneSekiro::AI(Role* r)
{}

int BattleSceneSekiro::checkResult()
{
    int team0 = getTeamMateCount(0);
    int team1 = enemies_.size() + getTeamMateCount(1);
    if (team0 > 0 && team1 == 0)
    {
        return 0;
    }
    if (team0 == 0 && team1 >= 0)
    {
        return 1;
    }
    return -1;
}

void BattleSceneSekiro::setRoleInitState(Role* r)
{
    BattleScene::setRoleInitState(r);
    if (r->Team == 0)
    {
        r->HP = r->MaxHP;
        r->MP = r->MaxMP;
        r->PhysicalPower = (std::max)(r->PhysicalPower, 90);
    }
    else
    {
        r->HP = r->MaxHP;
        r->MP = r->MaxMP;
        r->PhysicalPower = (std::max)(r->PhysicalPower, 90);
    }

    auto p = pos45To90(r->X(), r->Y());

    r->Pos.x = p.x;
    r->Pos.y = p.y;
    if (r->FaceTowards == Towards_RightDown)
    {
        r->RealTowards = { 1, 1 };
    }
    if (r->FaceTowards == Towards_RightUp)
    {
        r->RealTowards = { 1, -1 };
    }
    if (r->FaceTowards == Towards_LeftDown)
    {
        r->RealTowards = { -1, 1 };
    }
    if (r->FaceTowards == Towards_LeftUp)
    {
        r->RealTowards = { -1, -1 };
    }
    r->Acceleration = { 0, 0, gravity_ };
}

void BattleSceneSekiro::renderExtraRoleInfo(Role* r, double x, double y)
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
    BP_Rect r0 = { hp_x - 1, hp_y - 1, hp_max_w + 2, hp_h + 2 };
    Engine::getInstance()->renderSquareTexture(&r0, outline_color, 128 * alpha);
    BP_Rect r1 = { hp_x, hp_y, int(perc * hp_max_w), hp_h };
    Engine::getInstance()->renderSquareTexture(&r1, background_color, 192 * alpha);
}

Role* BattleSceneSekiro::findNearestEnemy(int team, Pointf p)
{
    double dis = 4096;
    Role* r0 = nullptr;
    for (auto r1 : battle_roles_)
    {
        if (r1->Dead == 0 && team != r1->Team)
        {
            auto dis1 = EuclidDis(p, r1->Pos);
            if (dis1 < dis)
            {
                dis = dis1;
                r0 = r1;
            }
        }
    }
    return r0;
}

Role* BattleSceneSekiro::findFarthestEnemy(int team, Pointf p)
{
    double dis = 0;
    Role* r0 = nullptr;
    for (auto r1 : battle_roles_)
    {
        if (r1->Dead == 0 && team != r1->Team)
        {
            auto dis1 = EuclidDis(p, r1->Pos);
            if (dis1 > dis)
            {
                dis = dis1;
                r0 = r1;
            }
        }
    }
    return r0;
}

int BattleSceneSekiro::calRolePic(Role* r, int style, int frame)
{
    if (style < 0 || style >= 5)
    {
        for (int i = 0; i < 5; i++)
        {
            if (r->FightFrame[i] > 0)
            {
                return r->FightFrame[i] * r->FaceTowards;
            }
        }
    }
    if (r->FightFrame[style] <= 0)
    {
        //改为选一个存在的动作，否则看不出是在攻击
        for (int i = 0; i < 5; i++)
        {
            if (r->FightFrame[i] > 0)
            {
                style = i;
            }
        }
    }
    int total = 0;
    for (int i = 0; i < 5; i++)
    {
        if (i == style)
        {
            if (frame < r->FightFrame[style] - 2)
            {
                return total + r->FightFrame[style] * r->FaceTowards + frame;
            }
            else
            {
                return total + r->FightFrame[style] * r->FaceTowards + r->FightFrame[style] - 2;
            }
        }
        total += r->FightFrame[i] * 4;
    }
    return r->FaceTowards;
}
