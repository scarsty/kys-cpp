#include "BattleSceneRTS.h"
#include "Audio.h"
#include "Event.h"
#include "Font.h"
#include "GameUtil.h"
#include "Head.h"
#include "MainScene.h"
#include "TeamMenu.h"
#include "Weather.h"

BattleSceneRTS::BattleSceneRTS()
{
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

    menu_ = std::make_shared<Menu>();
    menu_->setPosition(300, 30);
    button_magics_.resize(4);
    for (auto& em : button_magics_)
    {
        em = std::make_shared<Button>();
    }
    button_magics_[0]->setText("__________");
    menu_->addChild(button_magics_[0], 0, 10);
    button_magics_[1]->setText("__________");
    menu_->addChild(button_magics_[1], 120, 0);
    button_magics_[2]->setText("__________");
    menu_->addChild(button_magics_[2], 240, 10);
    button_magics_[3]->setText("__________");
    menu_->addChild(button_magics_[3], 120, 25);

    button_item_ = std::make_shared<Button>();
    button_item_->setText("__________");
    menu_->addChild(button_item_, 400, 10);
    addChild(menu_);
    menu_->setVisible(false);
    head_boss_.resize(6);
    for (auto& h : head_boss_)
    {
        h = std::make_shared<Head>();
        h->setStyle(1);
        h->setVisible(false);
        addChild(h);
    }
    show_auto_ = std::make_shared<TextBox>();
    //show_auto_->setHaveBox(false);
    show_auto_->setText("自動戰鬥");
    show_auto_->setFontSize(16);
    addChild(show_auto_);
    show_auto_->setPosition(20, 100);
    show_auto_->setVisible(false);
    makeSpecialMagicEffect();
}

BattleSceneRTS::BattleSceneRTS(int id) :
    BattleSceneRTS()
{
    setID(id);
}

BattleSceneRTS::~BattleSceneRTS()
{
}

void BattleSceneRTS::draw()
{
    layoutTeamHeads();
    drawClassicPresentation();
    for (auto& head : heads_)
    {
        drawSelectedHeadFrame(head);
    }
    for (auto& head : team_heads_)
    {
        drawSelectedHeadFrame(head);
    }
    if (selecting_)
    {
        auto rect = selectionRect();
        auto engine = Engine::getInstance();
        engine->renderSquareTexture(&rect, { 80, 220, 100, 255 }, 35);
        Rect edges[] = { { rect.x, rect.y, rect.w, 1 }, { rect.x, rect.y + rect.h, rect.w, 1 },
            { rect.x, rect.y, 1, rect.h }, { rect.x + rect.w, rect.y, 1, rect.h } };
        for (auto& edge : edges)
        {
            engine->renderSquareTexture(&edge, { 80, 220, 100, 255 }, 220);
        }
    }
}

Rect BattleSceneRTS::selectionRect() const
{
    return { (std::min)(selection_start_.x, selection_end_.x), (std::min)(selection_start_.y, selection_end_.y),
        abs(selection_end_.x - selection_start_.x), abs(selection_end_.y - selection_start_.y) };
}

void BattleSceneRTS::drawSelectedHeadFrame(const std::shared_ptr<Head>& head)
{
    auto role = head->getRole();
    if (!role || role->Dead || std::find(selected_roles_.begin(), selected_roles_.end(), role) == selected_roles_.end())
    {
        return;
    }
    int x = 0;
    int y = 0;
    head->getPosition(x, y);
    Rect edges[] = { { x - 2, y - 2, 254, 2 }, { x - 2, y + 90, 254, 2 },
        { x - 2, y, 2, 90 }, { x + 250, y, 2, 90 } };
    for (auto& edge : edges)
    {
        Engine::getInstance()->renderSquareTexture(&edge, { 80, 220, 100, 255 }, 255);
    }
}

void BattleSceneRTS::handleSelectionEvent(EngineEvent& e)
{
    if (result_ >= 0)
    {
        selecting_ = false;
        panning_ = false;
        return;
    }
    if (e.type == EVENT_WINDOW_FOCUS_LOST || e.type == EVENT_WINDOW_RESIZED)
    {
        selecting_ = false;
        panning_ = false;
    }
    if (e.type == EVENT_MOUSE_BUTTON_DOWN && e.button.button == BUTTON_RIGHT)
    {
        selecting_ = false;
        panning_ = true;
        pan_dragged_ = false;
        pan_start_ = { int(e.button.x), int(e.button.y) };
    }
    if (e.type == EVENT_MOUSE_BUTTON_UP && e.button.button == BUTTON_RIGHT)
    {
        if (panning_ && !pan_dragged_)
        {
            int mx = 0;
            int my = 0;
            auto engine = Engine::getInstance();
            if (engine->windowToUISpace(e.button.x, e.button.y, mx, my))
            {
                Role* target = nullptr;
                float nearest = 30.0f;
                for (auto r : battle_roles_)
                {
                    if (!r || r->Dead || r->Team == 0) { continue; }
                    float sx = (int(r->Pos.x) - int(pos_.x - render_center_x_)) * float(engine->getUIWidth()) / (2 * render_center_x_);
                    float sy = (int(r->Pos.y / 2 - r->Pos.z) - int(pos_.y / 2 - render_center_y_)) * float(engine->getUIHeight()) / (2 * render_center_y_);
                    float distance = std::hypot(sx - mx, sy - 25 - my);
                    if (distance < nearest)
                    {
                        nearest = distance;
                        target = r;
                    }
                }
                if (target)
                {
                    for (auto r : selected_roles_)
                    {
                        if (!r->Dead) { attack_targets_[r] = target; }
                    }
                }
            }
        }
        panning_ = false;
    }
    if (e.type == EVENT_MOUSE_MOTION && panning_)
    {
        if (!pan_dragged_ && std::hypot(e.motion.x - pan_start_.x, e.motion.y - pan_start_.y) < 5)
        {
            return;
        }
        pan_dragged_ = true;
        auto engine = Engine::getInstance();
        int viewport_x = 0;
        int viewport_y = 0;
        int viewport_width = 0;
        int viewport_height = 0;
        engine->getPresentRect(viewport_x, viewport_y, viewport_width, viewport_height);
        if (viewport_width <= 0 || viewport_height <= 0)
        {
            LOG("Cannot pan RTS camera: invalid viewport size {} x {}\n", viewport_width, viewport_height);
            panning_ = false;
            return;
        }
        constexpr float pan_speed = 2.0f;
        pos_.x -= e.motion.xrel * (2 * render_center_x_) / float(viewport_width) * pan_speed;
        pos_.y -= e.motion.yrel * (4 * render_center_y_) / float(viewport_height) * pan_speed;
        clampCameraPosition();
        return;
    }
    if (e.type != EVENT_MOUSE_BUTTON_DOWN && e.type != EVENT_MOUSE_BUTTON_UP && e.type != EVENT_MOUSE_MOTION)
    {
        return;
    }
    int mx = 0;
    int my = 0;
    int wx = e.type == EVENT_MOUSE_MOTION ? e.motion.x : e.button.x;
    int wy = e.type == EVENT_MOUSE_MOTION ? e.motion.y : e.button.y;
    if (!Engine::getInstance()->windowToUISpace(wx, wy, mx, my, selecting_))
    {
        return;
    }
    if (e.type == EVENT_MOUSE_BUTTON_DOWN && e.button.button == BUTTON_LEFT && !panning_)
    {
        for (auto& head : heads_)
        {
            if (head->inSide(wx, wy))
            {
                return;
            }
        }
        for (auto& head : team_heads_)
        {
            if (head->inSide(wx, wy))
            {
                return;
            }
        }
        selection_start_ = { mx, my };
        selection_end_ = selection_start_;
        selecting_ = true;
    }
    if (!selecting_)
    {
        return;
    }
    selection_end_ = { mx, my };
    if (e.type == EVENT_MOUSE_BUTTON_UP && e.button.button == BUTTON_LEFT)
    {
        auto rect = selectionRect();
        selected_roles_.clear();
        attack_targets_.clear();
        retaliation_targets_.clear();
        auto engine = Engine::getInstance();
        for (auto r : battle_roles_)
        {
            if (!r || r->Dead || r->Team != 0)
            {
                continue;
            }
            float sx = (int(r->Pos.x) - int(pos_.x - render_center_x_ - x_)) * float(engine->getUIWidth()) / (2 * render_center_x_);
            float sy = (int(r->Pos.y / 2) - int(pos_.y / 2 - render_center_y_ - y_)) * float(engine->getUIHeight()) / (2 * render_center_y_);
            if (sx >= rect.x && sx <= rect.x + rect.w && sy >= rect.y && sy <= rect.y + rect.h)
            {
                selected_roles_.push_back(r);
            }
        }
        selecting_ = false;
    }
}

void BattleSceneRTS::clampCameraPosition()
{
    pos_.x = std::clamp(pos_.x, float(render_center_x_), float((std::max)(render_center_x_, COORD_COUNT * TILE_W * 2 - render_center_x_)));
    pos_.y = std::clamp(pos_.y, float(render_center_y_ * 2), float((std::max)(render_center_y_ * 2, COORD_COUNT * TILE_H * 4 - render_center_y_ * 2)));
}

void BattleSceneRTS::updateEdgeScroll()
{
    const double now = Engine::getTicks();
    const double elapsed = std::clamp(now - camera_update_ticks_, 0.0, 50.0) / 1000.0;
    camera_update_ticks_ = now;
    auto engine = Engine::getInstance();
    if (selecting_ || panning_ || result_ >= 0
        || SDL_GetKeyboardFocus() != engine->getWindow() || SDL_GetMouseFocus() != engine->getWindow())
    {
        return;
    }
    int wx = 0;
    int wy = 0;
    int mx = 0;
    int my = 0;
    engine->getMouseState(wx, wy);
    if (!engine->windowToUISpace(wx, wy, mx, my))
    {
        return;
    }
    constexpr int edge_width = 20;
    constexpr float scroll_speed = 600.0f;
    Pointf direction;
    if (mx < edge_width)
    {
        direction.x = -1;
    }
    else if (mx >= engine->getUIWidth() - edge_width)
    {
        direction.x = 1;
    }
    if (my < edge_width)
    {
        direction.y = -1;
    }
    else if (my >= engine->getUIHeight() - edge_width)
    {
        direction.y = 1;
    }
    direction.normTo(scroll_speed * elapsed);
    pos_.x += direction.x * (2 * render_center_x_) / engine->getUIWidth();
    pos_.y += direction.y * (4 * render_center_y_) / engine->getUIHeight();
    clampCameraPosition();
}

void BattleSceneRTS::layoutTeamHeads()
{
    constexpr int team_head_width = 250;
    constexpr int team_head_right_margin = 10;
    constexpr int team_head_top_margin = 10;
    constexpr int team_head_row_height = 80;
    int ui_width = Engine::getInstance()->getUIWidth();
    int ui_height = Engine::getInstance()->getUIHeight();
    int x = ui_width - team_head_width - team_head_right_margin;
    for (int i = 0; i < int(team_heads_.size()); i++)
    {
        team_heads_[i]->setPosition(x, team_head_top_margin + team_head_row_height * i);
    }
    for (int i = 0; i < int(head_boss_.size()); i++)
    {
        auto& head = head_boss_[i];
        head->setPosition(ui_width / 2 - head->getWidth() / 2, ui_height - 50 - 25 * i);
    }
}

void BattleSceneRTS::dealEvent(EngineEvent& e)
{
    handleSelectionEvent(e);
    updateEdgeScroll();
    if (e.type == EVENT_KEY_UP && e.key.key == K_ESCAPE)
    {
        auto menu = std::make_shared<MenuText>();
        menu->setStrings({ "確認", "取消" });
        menu->setPosition(400, 300);
        menu->setFontSize(24);
        menu->setHaveBox(true);
        menu->setText("認輸？");
        menu->arrange(0, 50, 150, 0);
        if (menu->run() == 0)
        {
            result_ = 1;
            for (auto r : friends_)
            {
                r->ExpGot = 0;
            }
            setExit(true);
            return;
        }
    }
    simulation_tick_ = (simulation_tick_ + 1) % 2;
    if (simulation_tick_ != 0)
    {
        return;
    }
    simulation_frame_++;
    if (shake_ > 0)
    {
        x_ = rand_.rand_int(3) - rand_.rand_int(3);
        y_ = rand_.rand_int(3) - rand_.rand_int(3);
        shake_--;
    }
    else
    {
        x_ = 0;
        y_ = 0;
    }
    if (frozen_ > 0)
    {
        frozen_--;
        return;
    }
    decreaseToZero(close_up_);
    backRun1();
}

void BattleSceneRTS::dealEvent2(EngineEvent& e)
{
}

void BattleSceneRTS::onEntrance()
{
    camera_update_ticks_ = Engine::getTicks();
    simulation_tick_ = 0;
    simulation_frame_ = 0;
    selecting_ = false;
    panning_ = false;
    x_ = 0;
    y_ = 0;
    close_up_ = 0;
    shake_ = 0;
    dying_ = nullptr;
    selected_roles_.clear();
    calViewRegion();
    Audio::getInstance()->playMusic(info_->Music);
    //注意此时才能得到窗口的大小，用来设置头像的位置
    head_self_->setPosition(10, 10);
    int count = 0;
    for (auto& h : head_boss_)
    {
        h->setPosition(Engine::getInstance()->getUIWidth() / 2 - h->getWidth() / 2, Engine::getInstance()->getUIHeight() - 50 - (25 * count++));
    }
    addChild(Weather::getInstance());

    makeEarthTexture();    //注意高度稍微多了一点

    //此处创建了一个大的纹理，用于渲染整个场景
    Engine::getInstance()->createRenderedTexture("whole_scene", COORD_COUNT * TILE_W * 2, COORD_COUNT * TILE_H * 2);

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
            r->Auto = 1;
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
            team_heads_.push_back(head);
            addChild(head);
            i++;
        }
    }
    layoutTeamHeads();

    //head_self_->setRole(role_);

    for (int i = 0; i < button_magics_.size(); i++)
    {
        auto m = Save::getInstance()->getMagic(role_->EquipMagic[i]);
        if (m && role_->getMagicOfRoleIndex(m) < 0) { m = nullptr; }
        if (m)
        {
            std::string text = m->Name;
            text += std::string(10 - Font::getTextDrawSize(text), ' ');
            button_magics_[i]->setText(text);
        }
        else
        {
            button_magics_[i]->setText("__________");
        }
    }
    menu_->setVisible(false);
    pos_ = role_->Pos;
}

void BattleSceneRTS::onExit()
{
}

void BattleSceneRTS::backRun1()
{
    if (slow_ > 0)
    {
        if (simulation_frame_ % 4) { return; }
        slow_--;
    }
    for (auto r : battle_roles_)
    {
        r->HurtThisFrame = 0;
        for (auto m : r->getLearnedMagics())
        {
            if (special_magic_effect_every_frame_.count(m->Name))
            {
                special_magic_effect_every_frame_[m->Name](r);
            }
        }
        decreaseToZero(r->Frozen);
        decreaseToZero(r->Shake);
        if (r->Frozen > 0)
        {
            continue;
        }

        //更新速度，加速度，力学位置
        {
            auto p = r->Pos + r->Velocity;
            int dis = -1;
            if (r->OperationType == 3) { dis = 1; }
            if (canWalk90(p, r, dis))
            {
                r->Pos = p;
            }
            else
            {
                r->Velocity = { 0, 0, 0 };
            }
            //r->FaceTowards = rand_.rand() * 4;
            if (r->Pos.z < 0)
            {
                r->Pos.z = 0;
            }
            if (r->Pos.z == 0 && r->Velocity.norm() != 0)
            {
                auto f = -r->Velocity;
                f.normTo(friction_);
                r->Acceleration = { f.x, f.y, gravity_ };
            }
            else
            {
                r->Acceleration = { 0, 0, gravity_ };
            }
            r->Velocity += r->Acceleration;
            if (r->Pos.z == 0)
            {
                r->Velocity.z = 0;
            }
            if (r->Velocity.norm() < 0.1)
            {
                r->Velocity.x = 0;
                r->Velocity.y = 0;
            }
        }
        //else
        //{
        //    r->Velocity = { 0, 0 };
        //    if (r->HP <= 0)
        //    {
        //        r->Dead = 1;
        //        //此处只为严格化，但与击退部分可能冲突
        //    }
        //}
        decreaseToZero(r->CoolDown);
        if (r->CoolDown == 0)
        {
            if (simulation_frame_ % 3 == 0)
            {
                r->PhysicalPower += 1;
            }
            r->MP += 1;
            r->ActFrame = 0;
            //r->OperationType = -1;
            r->ActType = -1;
            r->HaveAction = 0;
        }
        decreaseToZero(r->HurtFrame);
        decreaseToZero(r->Attention);
        decreaseToZero(r->Invincible);
    }

    //if (simulation_frame_ % 2 == 0)
    {
        int current_frame2 = simulation_frame_;
        for (auto r : battle_roles_)
        {
            //有行动
            Action(r);
            //ai策略
            AI(r);
        }
    }

    //效果
    //if (simulation_frame_ % 2 == 0)
    {
        for (auto& ae : attack_effects_)
        {
            ae.Frame++;
            ae.Velocity += ae.Acceleration;
            ae.Pos += ae.Velocity;
            Role* r = nullptr;
            if (ae.Attacker)
            {
                r = findNearestEnemy(ae.Attacker->Team, ae.Pos);
            }
            if (ae.Track && r)
            {
                auto target = commandTarget(ae.Attacker);
                if (target)
                {
                    r = target;
                }
                //追踪
                double n = ae.Velocity.norm();
                auto p = (r->Pos - ae.Pos).normTo(n / 20.0);
                ae.Velocity += p;
                ae.Velocity.normTo(n);
                r = findNearestEnemy(ae.Attacker->Team, ae.Pos);
            }
            //是否打中了敌人
            if (r && !r->HurtFrame
                && !r->Invincible
                && r->Dead == 0
                && ae.Attacker
                && r->Team != ae.Attacker->Team
                && ae.Defender.count(r) == 0
                && EuclidDis(r->Pos, ae.Pos) <= TILE_W * 2)
            {
                if (ae.UsingMagic)
                {
                    Audio::getInstance()->playESound(ae.UsingMagic->EffectID);
                }
                ae.Defender[r]++;
                if (r->Team == 0)
                {
                    retaliation_targets_[r] = ae.Attacker;
                }
                r->Frozen = 20;
                r->Shake = 20;
                //slow_ = 1;
                if (ae.OperationType >= 0)
                {
                    //if (special_magic_effect_beat_.count(ae.UsingMagic->Name) == 0)
                    //{
                    defaultMagicEffect(ae, r);
                    //}
                    //else
                    //{
                    //    special_magic_effect_beat_[ae.UsingMagic->Name](ae, r);
                    //}
                }
                //std::vector<std::string> = {};
            }
        }
        //效果间的互相抵消
        if (attack_effects_.size() >= 2)
        {
            for (int i = 0; i < attack_effects_.size() - 2; i++)
            {
                auto& ae1 = attack_effects_[i];
                if (!ae1.UsingMagic)
                {
                    continue;
                }
                for (int j = i + 1; j < attack_effects_.size() - 1; j++)
                {
                    auto& ae2 = attack_effects_[j];
                    if (!ae2.UsingMagic)
                    {
                        continue;
                    }
                    if (ae1.NoHurt == 0 && ae2.NoHurt == 0 && ae1.Attacker && ae2.Attacker
                        && ae1.Attacker->Team != ae2.Attacker->Team && EuclidDis(ae1.Pos, ae2.Pos) < TILE_W * 4)
                    {
                        //LOG("{} beat {}, ", ae1.UsingMagic->Name, ae2.UsingMagic->Name);
                        int hurt1 = calMagicHurt(ae1.Attacker, ae2.Attacker, ae1.UsingMagic);
                        int hurt2 = calMagicHurt(ae2.Attacker, ae1.Attacker, ae2.UsingMagic);
                        ae1.Weaken += hurt2;
                        ae2.Weaken += hurt1;
                        if (ae1.Weaken > hurt1)
                        {
                            //直接设置帧数，后面就会删掉了
                            ae1.NoHurt = 1;
                            ae1.Frame = std::max(ae1.TotalFrame - 5, ae1.Frame);
                            //LOG("{} ", ae1.UsingMagic->Name);
                        }
                        if (ae2.Weaken > hurt2)
                        {
                            ae2.NoHurt = 1;
                            ae2.Frame = std::max(ae2.TotalFrame - 5, ae2.Frame);
                            //LOG("{} ", ae2.UsingMagic->Name);
                        }
                        //LOG("loss\n");
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
    }

    //此处计算累积伤害
    for (auto r : battle_roles_)
    {
        int hurt = r->HurtThisFrame;
        if (hurt > 0)
        {
            TextEffect te;
            Color c = { 255, 255, 255, 255 };
            if (r->Team == 0)
            {
                c = { 255, 20, 20, 255 };
            }
            te.set(std::to_string(-hurt), c, r);
            text_effects_.push_back(std::move(te));
            AttackEffect ae1;
            ae1.FollowRole = r;
            //ae1.EffectNumber = eft[rand_.rand() * eft.size()];
            ae1.setPath(std::format("eft/bld{:03}", int(rand_.rand() * 5)));
            ae1.TotalFrame = ae1.TotalEffectFrame;
            ae1.Frame = 0;
            attack_effects_.push_back(std::move(ae1));
            r->HP -= hurt;
            if (r->HP <= 0)
            {
                //LOG("{} has been beat\n", r->Name);
                r->Dead = 1;
                dying_ = r;
                r->HP = 0;
                //r->Velocity = r->Pos - ae1.Attacker->Pos;
                r->Velocity.normTo(15);    //因为已经有击退速度，可以直接利用
                r->Velocity.z = 12;
                //r->Velocity.normTo(std::min(hurt / 1.0, 15.0));
                r->Velocity.normTo(hurt / 2.0);
                r->Frozen = 5;
            }
        }
        r->HP = GameUtil::clamp(r->HP, 0, r->MaxHP);
        r->MP = GameUtil::clamp(r->MP, 0, r->MaxMP);
        r->PhysicalPower = GameUtil::clamp(r->PhysicalPower, 0, 100);
    }

    //处理文字
    {
        for (auto& te : text_effects_)
        {
            if (te.Type == 0)
            {
                te.Pos.y -= 2;
                te.PaperRise += 2;
            }
            te.Frame++;
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
                    head_boss_[i]->setVisible(false);
                }
            }
        }
        //检测战斗结果
        int battle_result = checkResult();

        if (battle_result >= 0)
        {
            if (result_ == -1)
            {
                if (dying_)
                {
                    pos_ = dying_->Pos;
                }
                close_up_ = 60;
                shake_ = 60;
                frozen_ = 60;
                slow_ = 40;
                result_ = battle_result;
            }
            if (slow_ == 0 && (result_ == 0 || result_ == 1))
            {
                menu_->setVisible(false);
                calExpGot();
                setExit(true);
            }
        }
    }
}

void BattleSceneRTS::Action(Role* r)
{
    if (r->HaveAction)
    {
        //音效和动画
        if (r->OperationType >= 0
            //&& r->ActFrame == r->FightFrame[r->ActType] - 3
            && r->ActFrame == calCast(r->ActType, r->OperationType, r))
        {
            //r->HaveAction = 0;
            r->PreActTimer = simulation_frame_;
            for (auto m : r->getLearnedMagics())
            {
                if (special_magic_effect_attack_.count(m->Name))
                {
                    special_magic_effect_attack_[m->Name](r);
                }
            }
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
                Audio::getInstance()->playASound(magic->SoundID);
                ae.setEft(magic->EffectID);
                ae.UsingMagic = magic;
            }
            else
            {
                Audio::getInstance()->playESound(r->ActType);
                ae.setEft(11);
                magic = Save::getInstance()->getMagic(1);
                ae.UsingMagic = Save::getInstance()->getMagic(1);
            }
            r->PhysicalPower = GameUtil::clamp(r->PhysicalPower - 3, 0, Role::getMaxValue()->PhysicalPower);
            int level_index = r->getMagicLevelIndex(magic->ID);
            int needMP = magic->calNeedMP(level_index);
            ae.TotalFrame = 30;
            //r->CoolDown += ae.TotalFrame;
            ae.Attacker = r;
            r->RealTowards.normTo(1);
            ae.Pos = r->Pos + TILE_W * 2.0 * r->RealTowards;
            ae.Frame = 0;
            if (r->Team == 0 && r == role_)
            {
                ae.OperationType = r->OperationType;
                //if (r->OperationType == 0 && ae.UsingMagic->AttackAreaType != 0)
                //{
                //    ae.OperationType = -1;
                //}
                //if (r->OperationType == 2 && (ae.UsingMagic->AttackAreaType != 1 && ae.UsingMagic->AttackAreaType != 2))
                //{
                //    ae.OperationType = -1;
                //}
                //if (r->OperationType == 1 && ae.UsingMagic->AttackAreaType != 3)
                //{
                //    ae.OperationType = -1;
                //}
            }
            else
            {
                ae.OperationType = r->OperationType;
                if (r->OperationType == -1)
                {
                    if (ae.UsingMagic->AttackAreaType == 0)
                    {
                        ae.OperationType = 0;
                    }
                    else if (ae.UsingMagic->AttackAreaType == 1 || ae.UsingMagic->AttackAreaType == 2)
                    {
                        ae.OperationType = 2;
                    }
                    else if (ae.UsingMagic->AttackAreaType == 3)
                    {
                        ae.OperationType = 1;
                    }
                }
            }

            int index = r->getMagicOfRoleIndex(ae.UsingMagic);
            if (index >= 0)
            {
                r->MagicLevel[index] = GameUtil::clamp(r->MagicLevel[index] + rand_.rand() * 2 + 1, 0, 999);
            }
            //根据性质创造攻击效果
            if (ae.OperationType == 0)
            {
                ae.TotalFrame = 10;
                if (r->OperationCount == 3 && magic->AttackAreaType == 0)
                {
                    ae.TotalFrame = 30;
                    ae.Strengthen = 2;
                    ae.Velocity = r->RealTowards;
                    ae.Velocity.normTo(magic->SelectDistance[level_index] / 2.0);
                    ae.Track = 1;
                }
                attack_effects_.push_back(ae);
                needMP *= 0.1;
            }
            else if (ae.OperationType == 1)
            {
                int range = 0;
                if (magic->AttackAreaType == 3)
                {
                    range = 1;
                    //range += magic->AttackDistance[level_index] + magic->SelectDistance[level_index] / 2;
                }
                int count = 1 + range;
                //count = 2;
                auto p = ae.Pos;
                ae.TotalFrame = 120;
                double angle = r->RealTowards.getAngle();
                for (int i = 0; i < count; i++)
                {
                    float a = angle - 5 / 180 * M_PI + rand_.rand() * 10 / 180 * M_PI;
                    ae.Pos = p;
                    ae.Velocity = { cos(a), sin(a) };
                    ae.Velocity.normTo(3);
                    ae.Frame = rand_.rand() * 10;
                    ae.Track = 1;
                    attack_effects_.push_back(ae);
                }
                needMP *= 0.4;
            }
            else if (ae.OperationType == 2)
            {
                auto r0 = commandTarget(r);
                if (r0)
                {
                    ae.Velocity = r0->Pos - r->Pos;
                    r->RealTowards = ae.Velocity;
                    //r->FaceTowards = realTowardsToFaceTowards(r->RealTowards);
                }
                else
                {
                    ae.Velocity = r->RealTowards;
                }
                ae.Velocity.normTo(5);
                ae.TotalFrame = 15 + magic->SelectDistance[level_index] * 5;
                if (magic->AttackAreaType == 1 || magic->AttackAreaType == 2)
                {
                    ae.Through = 1;
                }
                attack_effects_.push_back(ae);
                needMP *= 0.2;
                if (magic->AttackAreaType == 1 || magic->AttackAreaType == 2)
                {
                    double v = 5;
                    double angle = ae.Velocity.getAngle();
                    for (int i = 0; i < 2; i++)
                    {
                        v -= 0.5;
                        float a = angle - 15 / 180 * M_PI + rand_.rand() * 30 / 180 * M_PI;
                        ae.Velocity = { cos(a), sin(a) };
                        ae.Velocity.normTo(v);
                        //ae.TotalFrame = 150;
                        ae.Through = 1;
                        ae.Strengthen = 0.5;
                        attack_effects_.push_back(ae);
                        //ae.Pos = ae.Pos - ae.Velocity;
                    }
                }
            }
            else if (ae.OperationType == 3)
            {
                auto acc = r->RealTowards;
                acc.normTo(std::min(4.0, r->Speed / 30.0) * 1.7);
                r->Velocity = acc;
                //r->Acceleration += acc;
                //r->VelocitytFrame = 10;
                r->ActType = 0;
                auto p = ae.Pos;
                int count = std::min(3, (r->Speed + r->getActProperty(ae.UsingMagic->MagicType)) / 60);
                for (int i = 0; i < count; i++)
                {
                    ae.Pos = p + r->Velocity * (i - 1) * 2;
                    ae.Frame += 3;
                    attack_effects_.push_back(ae);
                }
                needMP *= 0.05;
            }
            LOG("{} use {} as {}\n", ae.Attacker->Name, ae.UsingMagic->Name, ae.OperationType);
            r->MP -= needMP;
            r->UsingMagic = nullptr;
        }

        if (r->UsingItem)
        {
            Item* item = r->UsingItem;
            if (item->ItemType == 3)
            {
                // 药品直接服用
                r->useItem(item);
                //TextEffect te;
                //BP_Color c = { 255, 255, 255, 255 };
                //if (r->Team == 0)
                //{
                //    c = { 255, 20, 220, 20 };
                //}
                //const int left = std::max(0, Save::getInstance()->getItemCountInBag(item->ID) - 1);
                //te.set(std::format("服用{}，剩余{}", item->Name, left), c, r);
                //text_effects_.push_back(std::move(te));
            }
            else if (item->ItemType == 4)
            {
                // 暗器
                AttackEffect ae1;
                auto r0 = commandTarget(r);
                if (r0)
                {
                    ae1.Velocity = r0->Pos - r->Pos;
                }
                else
                {
                    ae1.Velocity = r->RealTowards;
                }
                ae1.Velocity.normTo(10);
                ae1.Attacker = r;
                ae1.Pos = r->Pos;
                ae1.UsingHiddenWeapon = item;
                ae1.Through = 0;
                ae1.setEft(item->HiddenWeaponEffectID);
                ae1.TotalFrame = 100;
                ae1.Frame = 0;
                ae1.OperationType = 4;
                attack_effects_.push_back(std::move(ae1));
            }
            // 减少数量
            Event::getInstance()->addItemWithoutHint(item->ID, -1);
            r->UsingItem = nullptr;
        }

        r->ActFrame++;
    }
}

Role* BattleSceneRTS::commandTarget(Role* role)
{
    if (role->Team != 0)
    {
        return findNearestEnemy(role->Team, role->Pos);
    }
    for (auto targets : { &attack_targets_, &retaliation_targets_ })
    {
        auto it = targets->find(role);
        if (it == targets->end()) { continue; }
        auto target = it->second;
        if (std::find(battle_roles_.begin(), battle_roles_.end(), target) != battle_roles_.end()
            && !target->Dead && target->Team != role->Team)
        {
            return target;
        }
        targets->erase(it);
    }
    return nullptr;
}

void BattleSceneRTS::AI(Role* r)
{
    if (r->Team == 0 && !commandTarget(r))
    {
        return;
    }
    if ((r != role_ || r->Auto)
        && r->Dead == 0)
    {
        if (r->CoolDown == 0)
        {
            if (r->UsingMagic == nullptr)
            {
                if (r == role_)    //主角只能使用已装备的武学，几率相同
                {
                    std::vector<Magic*> v;
                    for (auto i : r->EquipMagic)
                    {
                        auto m = Save::getInstance()->getMagic(i);
                        if (m && r->getMagicOfRoleIndex(m) >= 0)
                        {
                            v.push_back(m);
                        }
                    }
                    if (!v.empty())
                    {
                        int index = rand_.rand() * v.size();
                        r->UsingMagic = v[index];
                    }
                }
                else
                {
                    //其他ai可以使用所有武学
                    auto v = r->getLearnedMagics();
                    if (v.size() == 1)
                    {
                        r->UsingMagic = v[0];
                    }
                    else if (v.size() >= 0)
                    {
                        std::vector<double> hurt;
                        double sum = 0;
                        for (auto m : v)
                        {
                            double h = m->Attack[r->getMagicLevelIndex(m)];
                            //h = exp(h / 500);    //几率正比于武功威力
                            hurt.push_back(sum + h);
                            sum += h;
                        }
                        double select = rand_.rand() * sum;
                        for (int i = 0; i < hurt.size(); i++)
                        {
                            if (select < hurt[i])
                            {
                                r->UsingMagic = v[i];
                                break;
                            }
                        }
                    }
                }
            }
            auto r0 = commandTarget(r);
            if (r0)
            {
                r->RealTowards = r0->Pos - r->Pos;
                //r->FaceTowards = realTowardsToFaceTowards(r->RealTowards);
                r->RealTowards.normTo(1);
                int dis = TILE_W * 3;
                if (r->UsingMagic)
                {
                    if (r->UsingMagic->AttackAreaType == 3) { dis = 180; }
                    if (r->UsingMagic->AttackAreaType == 1 || r->UsingMagic->AttackAreaType == 2) { dis = 300; }
                }
                double speed = r->Speed / 30.0;
                if (EuclidDis(r->Pos, r0->Pos) > dis)
                {
                    if (r->Team == 0 && attack_targets_.count(r) == 0)
                    {
                        return;
                    }
                    auto p = r->Pos + speed * r->RealTowards;
                    if (canWalk90(p, r) && r->FindingWay == 0)
                    {
                        //能否闪身的条件，似乎比较复杂
                        if (rand_.rand() < 0.25 && r->Speed >= 60
                            && (r != role_ && r->UsingMagic
                                || r == role_ && r->getEquipMagicOfRoleIndex(r->UsingMagic) == 3))
                        {
                            r->OperationType = 3;
                        }
                        else
                        {
                            r->OperationType = -1;
                        }
                        if (r->OperationType == 3)
                        {
                            r->CoolDown = calCoolDown(r->UsingMagic->MagicType, r->OperationType, r);
                            r->ActFrame = 0;
                            r->HaveAction = 1;
                        }
                        else
                        {
                            r->Pos = p;
                        }
                    }
                    else if (r->Velocity.norm() < 0.1)
                    {
                        //用复杂路径法查找一个目标并接近
                        MapSquareInt dis_layer;
                        dis_layer.resize(COORD_COUNT);
                        auto p_enemy45 = pos90To45(r0->Pos.x, r0->Pos.y);
                        calDistanceLayer(p_enemy45.x, p_enemy45.y, dis_layer, 64);
                        auto p_self45 = pos90To45(r->Pos.x, r->Pos.y);
                        int max_dis45 = 4096;
                        Pointf p_target = r->Pos;
                        for (int x = p_self45.x - 1; x <= p_self45.x + 1; x++)
                        {
                            for (int y = p_self45.y - 1; y <= p_self45.y + 1; y++)
                            {
                                if (isOutLine(x, y) || calDistance(x, y, p_self45.x, p_self45.y) != 1)
                                {
                                    continue;
                                }
                                auto p1 = pos45To90(x, y);
                                double dis1 = dis_layer.data(x, y) + 1 * (rand_.rand() - rand_.rand());
                                if (canWalk90(p1, r) && dis1 < max_dis45)
                                {
                                    max_dis45 = dis1;
                                    p_target = p1;
                                }
                            }
                        }
                        r->FindingWay = 1;
                        r->RealTowards = p_target - r->Pos;
                        if (rand_.rand() < 0.25 && r->Speed >= 60
                            && (r != role_ && r->UsingMagic
                                || r == role_ && r->getEquipMagicOfRoleIndex(r->UsingMagic) == 3))
                        {
                            r->OperationType = 3;
                        }
                        else
                        {
                            r->OperationType = -1;
                        }
                        if (r->OperationType == 3)
                        {
                            r->CoolDown = calCoolDown(r->UsingMagic->MagicType, r->OperationType, r);
                            r->ActFrame = 0;
                            r->HaveAction = 1;
                        }
                        else
                        {
                            r->RealTowards = p_target - r->Pos;
                            //r->FaceTowards = realTowardsToFaceTowards(r->RealTowards);
                            auto distance = r->RealTowards.norm();
                            //r->FaceTowards = readTowardsToFaceTowards(r->RealTowards);
                            r->RealTowards.normTo(1);
                            //r->Pos = p2;
                            r->Velocity = r->RealTowards * speed;
                            //todo:r->VelocitytFrame = 3;
                        }
                    }
                }
                else
                {
                    r->FindingWay = 0;
                    if (r->PhysicalPower >= 30 && r->UsingMagic)
                    {
                        //点攻击疯狗咬即可
                        if (r->UsingMagic->AttackAreaType == 0 || rand_.rand() < 0.75 && r->UsingMagic->AttackAreaType != 0)
                        {
                            //attack
                            auto m = r->UsingMagic;
                            if (m)
                            {
                                if (r == role_)
                                {
                                    r->OperationType = r->getEquipMagicOfRoleIndex(m);
                                }
                                else
                                {
                                    if (m->AttackAreaType == 0)
                                    {
                                        r->OperationType = 0;
                                    }
                                    else if (m->AttackAreaType == 1 || m->AttackAreaType == 2)
                                    {
                                        r->OperationType = 2;
                                    }
                                    else if (m->AttackAreaType == 3)
                                    {
                                        r->OperationType = 1;
                                    }
                                }
                                r->CoolDown = calCoolDown(m->MagicType, r->OperationType, r);
                                r->ActFrame = 0;
                                r->ActType = m->MagicType;
                                r->HaveAction = 1;
                                //TextEffect te;
                                //te.Text = m->Name;
                                //te.Size = 15;
                                //te.Type = 1;
                                //te.Pos.x = r->Pos.x - 15 * te.Text.size() / 3;
                                //te.Pos.y = r->Pos.y;
                                //te.Color = { 255, 0, 0, 255 };
                                //te.Frame = 15;
                                //text_effects_.push_back(te);
                            }
                        }
                        else
                        {
                            if (r->Team != 0 && r != role_ && rand_.rand() < 0.25)
                            {
                                r->OperationType = 3;
                            }
                            if (r->OperationType == 3)
                            {
                                //随机移动一下，增加一些变数
                                r->RealTowards.rotate(M_PI * 0.75 * (2 * rand_.rand() - 1));
                                r->OperationType = 3;
                                r->CoolDown = calCoolDown(r->UsingMagic->MagicType, r->OperationType, r);
                                r->ActFrame = 0;
                                r->HaveAction = 1;
                                //r->RealTowards *= 3;
                            }
                        }
                        if (!r->HaveAction && (r->Team != 0 || attack_targets_.count(r) != 0))
                        {
                            //走两步
                            r->RealTowards.rotate(M_PI * 0.5 * (2 * rand_.rand() - 1));
                            //r->FaceTowards = realTowardsToFaceTowards(r->RealTowards);
                            r->Velocity = r->RealTowards;
                            r->Velocity.normTo(speed);
                            //todo:r->VelocitytFrame = 20;
                        }
                    }
                }
            }
        }
    }
}

void BattleSceneRTS::onPressedCancel()
{
}

void BattleSceneRTS::renderExtraRoleInfo(Role* r, int x, int y)
{
    if (r == nullptr || r->Dead || std::find(selected_roles_.begin(), selected_roles_.end(), r) == selected_roles_.end())
    {
        return;
    }
    // 画个血条
    Color outline_color = { 0, 0, 0, 128 };
    Color background_color = { 0, 255, 0, 128 };    // 我方绿色
    if (r->Team == 1)
    {
        // 敌方红色
        background_color = { 255, 0, 0, 128 };
    }
    int tile_scale = (std::max)(1, TILE_W / TILE_W_0);
    int hp_max_w = 24 * tile_scale;
    int hp_x = x - hp_max_w / 2;
    int hp_y = y - 60 * tile_scale;
    int hp_h = 3 * tile_scale;
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
    Rect r0 = { hp_x - tile_scale, hp_y - tile_scale, hp_max_w + 2 * tile_scale, hp_h + 2 * tile_scale };
    Engine::getInstance()->renderSquareTexture(&r0, outline_color, 128 * alpha);
    Rect r1 = { hp_x, hp_y, int(perc * hp_max_w), hp_h };
    Engine::getInstance()->renderSquareTexture(&r1, background_color, 192 * alpha);
}

int BattleSceneRTS::checkResult()
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

void BattleSceneRTS::setRoleInitState(Role* r)
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

Role* BattleSceneRTS::findNearestEnemy(int team, Pointf p)
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

Role* BattleSceneRTS::findFarthestEnemy(int team, Pointf p)
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

//前摇
int BattleSceneRTS::calCast(int act_type, int operation_type, Role* r)
{
    int v[4] = { 10, 20, 15, 5 };
    if (operation_type >= 0 && operation_type <= 3)
    {
        return v[operation_type];
    }
    return 0;
}

//冷却减去前摇就是后摇
//需注意攻击判定可能仍然存在，严格来说攻击判定存在的时间加上前摇应小于冷却
int BattleSceneRTS::calCoolDown(int act_type, int operation_type, Role* r)
{
    int i = r->getActProperty(act_type);
    int v[4] = { 60 - i / 2, 160 - i, 70 - i / 2, 10 };
    int min_v[4] = { 20, 45, 30, 10 };
    if (operation_type >= 0 && operation_type <= 3)
    {
        int c = std::max(min_v[operation_type], v[operation_type]);
        if (r->AttackTwice > 0)
        {
            c *= 0.666;
            c = std::max(calCast(act_type, operation_type, r) + 2, c);
        }
        return c;
    }
    else
    {
        return 0;
    }
}

void BattleSceneRTS::defaultMagicEffect(AttackEffect& ae, Role* r)
{
    if (ae.NoHurt)
    {
        return;
    }
    double hurt;
    //先特别处理暗器
    if (ae.UsingHiddenWeapon != nullptr)
    {
        hurt = calHiddenWeaponHurt(ae.Attacker, r, ae.UsingHiddenWeapon) / 5;
    }
    else
    {
        hurt = calMagicHurt(ae.Attacker, r, ae.UsingMagic);
    }
    hurt -= ae.Weaken;                             //弱化
    hurt *= ae.Strengthen;                         //强化
    hurt *= 1 - 0.5 * ae.Frame / ae.TotalFrame;    //距离衰减
    //角度
    auto atk_dir = ae.Pos - r->Pos;
    auto angle = acos((atk_dir.x * r->RealTowards.x + atk_dir.y * r->RealTowards.y) / atk_dir.norm() / r->RealTowards.norm());
    if (angle >= M_PI * 0.25 && angle < M_PI * 0.75)
    {
        hurt *= 1.5;
    }
    else if (angle >= M_PI * 0.75)
    {
        hurt *= 2;
    }
    //操作类型的伤害效果
    if (ae.OperationType == 0)
    {
    }
    if (ae.OperationType == 1)
    {
        hurt *= 1.5;
        //ae.Frame = ae.TotalFrame + 1;
    }
    if (ae.OperationType == 2)
    {
        //hurt /= 3;
        //ae.Frame = ae.TotalFrame + 1;
    }
    if (ae.OperationType == 3)
    {
        hurt /= 1.5;
        r->Frozen += 5;
    }
    //击退
    auto v = r->Pos - ae.Attacker->Pos;
    v.normTo(2);
    r->Velocity += v;
    if (r->Velocity.norm() > 3)
    {
        r->Velocity.normTo(3);
    }
    r->HurtFrame = 1;    //相当于无敌时间

    //用内力抵消硬直
    if (r->MP >= hurt / 10)
    {
        r->MP -= hurt / 10;
    }
    else
    {
        r->Frozen += 10;    //硬直
        //r->ActType = -1;
        //r->ActType2 = -1;
    }

    //武功类型特殊效果
    if (ae.UsingMagic)
    {
        int act_type = ae.UsingMagic->MagicType;
        if (rand_.rand() < r->getActProperty(act_type) / 200.0)
        {
            if (act_type == 1)
            {
                r->Frozen += 10;    //拳法打硬直
            }
            if (act_type == 2)
            {
                ae.Attacker->CoolDown *= 0.5;    //剑法冷却缩短
            }
            if (act_type == 3)
            {
                hurt *= 1.5;    //刀法暴击
            }
            if (act_type == 4)
            {
                //特殊会随机附加行动方向
                Pointf p{ float(rand_.rand()), float(rand_.rand()), 0 };
                p.normTo(1);
                r->Velocity += p;
            }
        }
    }
    //添加一点随机性
    hurt += 5 * (rand_.rand() - rand_.rand());
    //若无法破防，则随机一个小的数字
    if (hurt <= 0)
    {
        hurt = 1 + rand_.rand() * 3;
    }
    //无贯穿则后面不会再造成伤害，再播放一下
    if (ae.Through == 0)
    {
        ae.NoHurt = 1;
        ae.Frame = std::max(ae.TotalFrame - 15, ae.Frame);
    }
    ae.Attacker->ExpGot += hurt / 2;
    //扣HP或MP
    if (ae.UsingHiddenWeapon || ae.UsingMagic->HurtType == 0)
    {
        r->HurtThisFrame += hurt;
    }
    if (ae.UsingMagic && ae.UsingMagic->HurtType == 1)
    {
        r->MP -= hurt;
        ae.Attacker->MP += hurt * 0.8;
        TextEffect te;
        te.set(std::to_string(int(-hurt)), { 160, 32, 240, 255 }, r);
        text_effects_.push_back(std::move(te));
    }
    //LOG("{} attack {} with {} as {}, hurt {}\n", ae.Attacker->Name, r->Name, ae.UsingMagic->Name, ae.OperationType, int(hurt));
}

int BattleSceneRTS::calRolePic(Role* r, int style, int frame)
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
            //停留在最后一帧
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

void BattleSceneRTS::makeSpecialMagicEffect()
{
}
