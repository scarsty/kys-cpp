#include "SubScene.h"
#include "Audio.h"
#include "BattleScene.h"
#include "ChessBalance.h"
#include "ChessManager.h"
#include "ChessModHook.h"
#include "ChessNeigong.h"
#include "ChessSelector.h"
#include "ChessUiCommon.h"
#include "Console.h"
#include "Event.h"
#include "Font.h"
#include "MainScene.h"
#include "ParticleExample.h"
#include "PotConv.h"
#include "Random.h"
#include "Timer.h"
#include "GameState.h"
#include "TextureManager.h"
#include "UI.h"
#include "Weather.h"
#include <format>

SubScene::SubScene()
{
    full_window_ = 1;
    COORD_COUNT = SUBMAP_COORD_COUNT;
    chess_mod_ = std::make_unique<KysChess::ChessMod>(KysChess::GameState::get());
    cloud_group_ = std::make_shared<CloudGroup>();
    cloud_group_->init(2, COORD_COUNT * TILE_W * 2, COORD_COUNT * TILE_H * 2);
    addChild(cloud_group_);
}

SubScene::SubScene(int id) :
    SubScene()
{
    setID(id);
}

SubScene::~SubScene()
{
}

void SubScene::setID(int id)
{
    submap_id_ = id;
    submap_info_ = Save::getInstance()->getSubMapInfo(submap_id_);
    if (submap_info_ == nullptr)
    {
        setExit(true);
    }
    //submap_info_->ID = submap_id_;    //这句是修正存档中可能存在的错误
    exit_music_ = submap_info_->ExitMusic;
    Audio::getInstance()->playMusic(KysChess::getRandomChessMusic());
    LOG("Sub Scene {}, {}\n", submap_id_, submap_info_->Name);
}

void SubScene::draw()
{
    //int k = 0;
    struct DrawInfo
    {
        int i;
        Point p;
    };

    //std::map<int, DrawInfo> map;

    Engine::getInstance()->setRenderTarget("scene");
    int w, h;
    Engine::getInstance()->getAssistTextureSize("scene", w, h);
    Engine::getInstance()->fillColor({ 0, 0, 0, 255 }, 0, 0, w, h);

    //Timer t0;
    //一整块地面
    //如果没有创建地面，那么这个值是空，即使用之前的画法
    auto earth_texture = Engine::getInstance()->getTexture("searth");
    if (earth_texture)
    {
        auto p = getPositionOnWholeEarth(view_x_, view_y_);
        int w = render_center_x_ * 2;
        int h = render_center_y_ * 2;
        //获取的是中心位置，如贴图应减掉屏幕尺寸的一半
        Rect rect0 = { p.x - render_center_x_, p.y - render_center_y_, w, h }, rect1 = { 0, 0, w, h };
        if (rect0.x < 0)
        {
            rect1.x = -rect0.x;
            rect0.x = 0;
        }
        if (rect0.y < 0)
        {
            rect1.y = -rect0.y;
            rect0.y = 0;
        }
        std::vector<Color> color_v(4, { 255, 255, 255, 255 });
        Engine::getInstance()->renderTextureLight(earth_texture, &rect0, &rect1, color_v, { 0.25f, 0.0f, 0.0f, 0.0f });
    }
    else
    {
        //#ifndef _DEBUG
        for (int sum = -view_sum_region_; sum <= view_sum_region_ + 15; sum++)
        {
            for (int i = -view_width_region_; i <= view_width_region_; i++)
            {
                int ix = view_x_ + i + (sum / 2);
                int iy = view_y_ - i + (sum - sum / 2);
                auto p = getPositionOnRender(ix, iy, view_x_, view_y_);
                p.x += x_;
                p.y += y_;
                if (!isOutLine(ix, iy))
                {
                    int h = submap_info_->BuildingHeight(ix, iy);
                    int num = submap_info_->Earth(ix, iy) / 2;
                    //低高度地面
                    if (num > 0 && h <= 2)
                    {
                        if (earth_texture == nullptr)
                        {
                            TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y);
                        }
                        else
                        {
                            auto tex = TextureManager::getInstance()->getTexture("smap", num);
                            //用大图画时的闪烁地面
                            if (tex->count > 1)
                            {
                                TextureManager::getInstance()->renderTexture(tex, p.x, p.y);
                            }
                        }
                    }
                }
            }
        }
        //#endif
    }

    for (int sum = -view_sum_region_; sum <= view_sum_region_ + 20; sum++)
    {
        for (int i = -view_width_region_; i <= view_width_region_; i++)
        {
            int ix = view_x_ + i + (sum / 2);
            int iy = view_y_ - i + (sum - sum / 2);
            auto p = getPositionOnRender(ix, iy, view_x_, view_y_);
            p.x += x_;
            p.y += y_;
            if (!isOutLine(ix, iy))
            {
                //有高度地面
                int rate = TILE_W / 18;                                 //高清模式要乘以倍率
                int h = submap_info_->BuildingHeight(ix, iy) * rate;    //高清模式要乘以倍率
                int num = submap_info_->Earth(ix, iy) / 2;
                // TODO: legacy device only
                if (GameUtil::isLegacyBrowser() || (num > 0 && h > 2 && !earth_texture))
                {
                    TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y);
                }
                //鼠标位置
                if (ix == cursor_x_ && iy == cursor_y_)
                {
                    TextureManager::getInstance()->renderTexture("smap", 1, p.x, p.y - h, TextureManager::RenderInfo{ { 255, 255, 255, 255 }, 128 });
                }
                std::vector<Color> color_v(4, { 255, 255, 255, 255 });
                std::vector<float> brightness_v = { 0.0f, 0.0f, 0.0f, 0.0f };
                //建筑和主角
                num = submap_info_->Building(ix, iy) / 2;
                if (num > 0)
                {
                    TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y - h,
                        TextureManager::RenderInfo{ { 255, 255, 255, 255 }, 255, 1, 1, 0, 0, color_v, brightness_v });
                }
                if (ix == man_x_ && iy == man_y_)
                {
                    //当主角的贴图为-1时，表示强制设置贴图号
                    if (force_man_pic_ == -1)
                    {
                        man_pic_ = calManPic();
                    }
                    else
                    {
                        man_pic_ = force_man_pic_;
                    }
                    TextureManager::getInstance()->renderTexture("smap", man_pic_, p.x, p.y - h,
                        TextureManager::RenderInfo{ { 255, 255, 255, 255 }, 255, 1, 1, 0, 0, color_v, brightness_v });
                }
                //事件
                auto event = submap_info_->Event(ix, iy);
                if (event)
                {
                    num = submap_info_->Event(ix, iy)->CurrentPic / 2;
                    //map[calBlockTurn(i1, i2, 2)] = s;
                    if (num > 0)
                    {
                        TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y - h,
                            TextureManager::RenderInfo{ { 255, 255, 255, 255 }, 255, 1, 1, 0, 0, color_v, brightness_v });
                    }
                }
                //装饰
                num = submap_info_->Decoration(ix, iy) / 2;
                if (num > 0)
                {
                    TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y - submap_info_->DecorationHeight(ix, iy) * rate,
                        TextureManager::RenderInfo{ { 255, 255, 255, 255 }, 255, 1, 1, 0, 0, color_v, brightness_v });
                }
            }
            //k++;
        }
    }
    Engine::getInstance()->renderTextureToMain("scene");

    // Chess mod header bar
    if (submap_id_ == 53)
    {
        using namespace KysChess;
        auto& gd = GameState::get();
        auto& economy = gd.economy();
        auto& progress = gd.progress();
        auto& cfg = ChessBalance::config();
        auto* engine = Engine::getInstance();
        auto* font = Font::getInstance();
        int w = engine->getUIWidth(), fs = 18, x = 10, y = 6;

        engine->fillColor({0, 0, 0, 180}, 0, 0, w, 32);

        auto seg = [&](const std::string& s, Color c) {
            font->draw(s, fs, x, y, c);
            x += fs * Font::getTextDrawSize(s) / 2 + 12;
        };

        int fight = progress.battleProgress().getFight();
        seg(std::format("第{}關{}", fight + 1, progress.battleProgress().isBossFight() ? "(Boss)" : ""), {255, 200, 100, 255});
        seg(std::format("${}", economy.getMoney()), {255, 215, 0, 255});
        seg(std::format("Lv{} {}/{}", economy.getLevel() + 1, economy.getExp(), economy.getExpForNextLevel()), {100, 200, 255, 255});
        auto chessManager = KysChess::ChessManager(gd.roster(), gd.equipmentInventory(), gd.economy());
        seg(std::format("出戰{}/{}", chessManager.getSelectedCount(), economy.getMaxDeploy()), {100, 255, 100, 255});
        seg(std::format("背包{}/{}", chessManager.getBenchCount(), cfg.benchSize), {200, 180, 255, 255});
        const char* diffName = gd.difficulty() == Difficulty::Easy ? "普通" : "困難";
        seg(std::format("[{}]", diffName), {255, 150, 150, 255});

        // Quick-access chess button (bottom-right of screen)
        if (!chess_menu_active_)
        {
            int btnFs = 70;
            std::string label = "[Q] 棋局";
            auto boxRect = Font::getBoxSize(Font::getTextDrawSize(label), btnFs, 0, 0);
            int labelW = boxRect.w;
            int labelH = boxRect.h;
            int h = engine->getUIHeight();
            int labelX = w - labelW - 20;
            int labelY = h / 2 - labelH / 2;
            // Store for click detection
            chess_btn_x_ = labelX;
            chess_btn_y_ = labelY;
            chess_btn_w_ = labelW;
            chess_btn_h_ = labelH;
            // Check hover
            int mx, my;
            engine->getMouseStateInStartWindow(mx, my);
            bool hovered = (mx >= labelX && mx < labelX + labelW
                && my >= labelY && my < labelY + labelH);
            if (hovered)
            {
                // Hover: brighter box + outline glow
                font->drawWithBox(label, btnFs, labelX + 10, labelY + 3, {255, 255, 200, 255}, 255, 240);
                Color outline = { 255, 225, 150, 200 };
                engine->drawRoundedRect(outline, labelX, labelY, labelW, labelH, 6);
            }
            else
            {
                font->drawWithBox(label, btnFs, labelX + 10, labelY + 3, {255, 255, 255, 255}, 255, 220);
            }
        }

        // Obtained neigong icons (right-aligned)
        auto& obtained = gd.progress().getObtainedNeigong();
        auto& pool = ChessNeigong::getPool();
        int iconX = w - 10;
        for (int i = (int)obtained.size() - 1; i >= 0; --i)
            for (auto& ng : pool)
                if (ng.magicId == obtained[i])
                {
                    iconX -= 22;
                    TextureManager::getInstance()->renderTexture("item", ng.itemId, iconX, 4,
                        TextureManager::RenderInfo{ { 255, 255, 255, 255 }, 255, 0.35, 0.35 });
                    break;
                }
    }

    //LOG("%g\n", t0.getElapsedTime());
}

void SubScene::dealEvent(EngineEvent& e)
{
    auto engine = Engine::getInstance();
    //实际上很大部分与大地图类似，这里暂时不合并了，就这样
    int x = man_x_, y = man_y_;

    if (checkEvent3(x, y))
    {
        //way_que_.clear();
    }
    if (isExit(x, y) || isJumpSubScene(x, y))
    {
        way_que_.clear();
        //clearEvent(e);
        total_step_ = 0;
    }
    // Tab激活控制台
    if (e.type == EVENT_KEY_UP && e.key.key == K_TAB
        || (e.type == EVENT_GAMEPAD_BUTTON_UP && e.gbutton.button == GAMEPAD_BUTTON_BACK))
    {
        //Event::getInstance()->tryBattle(1, 0);
        Console c;
    }
    // Quick-access chess shortcut (Q key or click on HUD button)
    if (submap_id_ == 53)
    {
        bool chessTriggered = false;
        if (e.type == EVENT_KEY_UP && e.key.key == K_Q)
        {
            chessTriggered = true;
        }
        if (e.type == EVENT_MOUSE_BUTTON_UP && e.button.button == BUTTON_LEFT)
        {
            int mx, my;
            Engine::getInstance()->getMouseStateInStartWindow(mx, my);
            if (mx >= chess_btn_x_ && mx < chess_btn_x_ + chess_btn_w_
                && my >= chess_btn_y_ && my < chess_btn_y_ + chess_btn_h_)
            {
                chessTriggered = true;
            }
        }
        if (chessTriggered)
        {
            // Find the first NPC event and teleport beside it
            for (int i = 0; i < SUBMAP_EVENT_COUNT; i++)
            {
                auto ev = submap_info_->Event(i);
                if (ev && ev->Event1 > 0)
                {
                    int ex = ev->X(), ey = ev->Y();
                    // Teleport player one tile away (try south/east/west/north)
                    static const int dx[] = { 0, 1, -1, 0 };
                    static const int dy[] = { 1, 0, 0, -1 };
                    for (int d = 0; d < 4; d++)
                    {
                        int tx = ex + dx[d], ty = ey + dy[d];
                        if (canWalk(tx, ty))
                        {
                            setManViewPosition(tx, ty);
                            towards_ = calTowards(tx, ty, ex, ey);
                            break;
                        }
                    }
                    break;
                }
            }
            chess_menu_active_ = true;
            chess_mod_->showContextMenu();
            chess_menu_active_ = false;
        }
    }
    if ((e.type == EVENT_KEY_UP && e.key.key == K_ESCAPE)
        || (e.type == EVENT_MOUSE_BUTTON_UP && e.button.button == BUTTON_RIGHT)
        //|| (e.type == EVENT_GAMEPAD_BUTTON_UP && e.gbutton.button == GAMEPAD_BUTTON_START)
        || engine->gameControllerGetButton(GAMEPAD_BUTTON_START))
    {
        chess_mod_->showMenu();
    }

    //键盘走路部分，检测4个方向键
    if (engine->getTicks() - pre_pressed_ticks_ > key_walk_delay_)
    {
        int64_t pressed = 0;
        pre_pressed_ticks_ = engine->getTicks();
        auto axis_x = engine->gameControllerGetAxis(GAMEPAD_AXIS_LEFTX);
        auto axis_y = engine->gameControllerGetAxis(GAMEPAD_AXIS_LEFTY);
        if (abs(axis_x) < 10000) { axis_x = 0; }
        if (abs(axis_y) < 10000) { axis_y = 0; }
        if (axis_x != 0 || axis_y != 0)
        {
            Pointf axis{ float(axis_x), float(axis_y) };
            auto to = realTowardsToFaceTowards(axis);
            if (to == Towards_LeftUp) { pressed = K_LEFT; }
            if (to == Towards_LeftDown) { pressed = K_DOWN; }
            if (to == Towards_RightDown) { pressed = K_RIGHT; }
            if (to == Towards_RightUp) { pressed = K_UP; }
        }
        if (engine->gameControllerGetButton(GAMEPAD_BUTTON_DPAD_LEFT)) { pressed = K_LEFT; }
        if (engine->gameControllerGetButton(GAMEPAD_BUTTON_DPAD_DOWN)) { pressed = K_DOWN; }
        if (engine->gameControllerGetButton(GAMEPAD_BUTTON_DPAD_RIGHT)) { pressed = K_RIGHT; }
        if (engine->gameControllerGetButton(GAMEPAD_BUTTON_DPAD_UP)) { pressed = K_UP; }
        if (engine->checkKeyPress(K_A)) { pressed = K_LEFT; }
        if (engine->checkKeyPress(K_S)) { pressed = K_DOWN; }
        if (engine->checkKeyPress(K_D)) { pressed = K_RIGHT; }
        if (engine->checkKeyPress(K_W)) { pressed = K_UP; }

        for (auto i = int(K_RIGHT); i <= int(K_UP); i++)
        {
            if (i != pre_pressed_ && Engine::getInstance()->checkKeyPress(i))
            {
                pressed = i;
            }
        }
        if (pressed == 0 && Engine::getInstance()->checkKeyPress(pre_pressed_))
        {
            pressed = pre_pressed_;
        }
        pre_pressed_ = pressed;

        if (pressed)
        {
            if (total_step_ < 1 || total_step_ >= first_step_delay_)
            {
                changeTowardsByKey(pressed);
                getTowardsPosition(man_x_, man_y_, towards_, &x, &y);
                tryWalk(x, y);
            }
            total_step_++;
        }
        else
        {
            if (rest_time_ > 2) { total_step_ = 0; }
        }
        if (!way_que_.empty())
        {
            Point p = way_que_.back();
            x = p.x;
            y = p.y;
            if (calDistance(man_x_, man_y_, x, y) <= 1)
            {
                auto tw = calTowards(man_x_, man_y_, x, y);
                if (tw != Towards_None)
                {
                    towards_ = tw;
                }
                tryWalk(x, y);
                way_que_.pop_back();
                if (way_que_.empty() && mouse_event_x_ >= 0 && mouse_event_y_ >= 0)
                {
                    auto tw = calTowards(man_x_, man_y_, mouse_event_x_, mouse_event_y_);
                    if (tw != Towards_None)
                    {
                        towards_ = tw;
                    }
                    checkEvent1(man_x_, man_y_, towards_);
                    setMouseEventPoint(-1, -1);
                }
            }
            else
            {
                way_que_.clear();
            }
            //if (isExit(x, y)) { way_que_.clear(); }
        }
    }
    //检查触发剧情事件
    if ((e.type == EVENT_KEY_UP && (e.key.key == K_RETURN || e.key.key == K_SPACE))
        || (e.type == EVENT_GAMEPAD_BUTTON_UP && e.gbutton.button == GAMEPAD_BUTTON_SOUTH))
    {
        if (checkEvent1(x, y, towards_))
        {
            //clearEvent(e);
            step_ = 0;
        }
    }

    calCursorPosition(view_x_, view_y_);

    //鼠标寻路
    if (e.type == EVENT_MOUSE_BUTTON_UP && e.button.button == BUTTON_LEFT)
    {
        if (GameUtil::isMobileDevice() && Engine::getTicks() - GameUtil::lastDialogDismissTime() < GameUtil::DIALOG_DISMISS_DELAY_MS)
        {
            return;
        }
        setMouseEventPoint(-1, -1);
        int mx, my;
        Engine::getInstance()->getMouseStateInStartWindow(mx, my);
        Point p = getMousePosition(mx, my, x, y);
        way_que_.clear();
        if (isCannotPassEvent(p.x, p.y))    //存在事件点则仅会走到倒数第二格
        {
            FindWay(x, y, p.x, p.y);
            if (way_que_.size() >= 1)
            {
                way_que_ = std::vector<Point>(way_que_.begin() + 1, way_que_.end());
                setMouseEventPoint(p.x, p.y);
            }
        }
        else if (isCanPassEvent1(p.x, p.y) && calDistance(p.x, p.y, x, y) == 1)    //身边的特殊事件点，仅用于硝石
        {
            FindWay(x, y, x, y);
            setMouseEventPoint(p.x, p.y);
        }
        else if (canWalk(p.x, p.y) /* && !isOutScreen(p.x, p.y)*/)
        {
            FindWay(x, y, p.x, p.y);
        }
    }
}

void SubScene::backRun()
{
    rest_time_++;
    //停止走动一段时间恢复站立姿势
    if (rest_time_ > 50)
    {
        step_ = 0;
    }
    if (current_frame_ % int(200 / RunNode::getRefreshInterval()) == 0)
    {
        for (int i = 0; i < SUBMAP_EVENT_COUNT; i++)
        {
            auto e = submap_info_->Event(i);
            if (e->BeginPic < e->EndPic)
            {
                e->CurrentPic++;
                if (e->CurrentPic > e->EndPic)
                {
                    e->CurrentPic = e->BeginPic;
                }
            }
        }
    }
    //LOG("sub scene %d,", current_frame_);
    cloud_group_->setPositionOnScreen(man_x_, man_y_, render_center_x_, render_center_y_);
}

void SubScene::onEntrance()
{
    calViewRegion();
    towards_ = MainScene::getInstance()->towards_;

    for (int i = 0; i < SUBMAP_EVENT_COUNT; i++)
    {
        auto e = submap_info_->Event(i);
        if (e->BeginPic < e->EndPic)
        {
            e->CurrentPic = e->BeginPic + e->PicDelay * 2 % (e->EndPic - e->BeginPic);
        }
    }

    if (force_begin_event_ >= 0)
    {
        Event::getInstance()->callEvent(force_begin_event_, this);
    }
    //setManViewPosition(submap_info_->EntranceX, submap_info_->EntranceY);
    //RunElement::addOnRootTop(MainScene::getInstance()->getWeather());
    addChild(Weather::getInstance());
    //fillEarth();

    //一大块地面的纹理，预先拼好地面，可以减少绘制的次数
    //暂时不使用这种方法，地面的动态效果会消失
    if (GameUtil::isLegacyBrowser())
    {
        Engine::getInstance()->destroyTexture("searth");
    }
    else
    {
        Engine::getInstance()->createRenderedTexture("searth", COORD_COUNT * TILE_W * 2, COORD_COUNT * TILE_H * 2);
        reDrawEarthTexture();
    }
    //Engine::getInstance()->saveTexture(earth_texture, std::format("{}.bmp", submap_id_).c_str());

    // auto scene_name = std::make_shared<TextBox>();
    // scene_name->setFontSize(24);
    // scene_name->setText(submap_info_->Name);
    // scene_name->setPosition(Engine::getInstance()->getUIWidth() / 2 - Font::getTextDrawSize(submap_info_->Name) / 2 * 12, 100);
    // scene_name->setStayFrame(40);
    // addChild(scene_name);

    chess_mod_->onSubSceneEntrance(submap_id_);
}

void SubScene::onExit()
{
    Audio::getInstance()->playMusic(exit_music_);
}

void SubScene::onPressedCancel()
{
}

//冗余过多待清理
void SubScene::tryWalk(int x, int y)
{
    if (canWalk(x, y))
    {
        man_x_ = x;
        man_y_ = y;
        view_x_ = x;
        view_y_ = y;
    }
    step_++;
    if (step_ >= MAN_PIC_COUNT)
    {
        step_ = 1;
    }
    rest_time_ = 0;
}

bool SubScene::checkEvent(int x, int y, int tw /*= None*/, int item_id /*= -1*/)
{
    getTowardsPosition(man_x_, man_y_, tw, &x, &y);
    int event_index_submap = submap_info_->EventIndex(x, y);
    if (event_index_submap >= 0)
    {
        int id = 0;
        if (tw != Towards_None)
        {
            if (item_id < 0)
            {
                id = submap_info_->Event(x, y)->Event1;
            }
            else
            {
                id = submap_info_->Event(x, y)->Event2;
            }
            if (id > 0)
            {
                step_ = 0;
            }
        }
        else
        {
            id = submap_info_->Event(x, y)->Event3;
        }
        if (id > 0)
        {
            chess_menu_active_ = true;
            bool intercepted = chess_mod_->interceptEvent(submap_info_->ID, id);
            chess_menu_active_ = false;
            if (intercepted) { return true; }
            return Event::getInstance()->callEvent(id, this, submap_info_->ID, item_id, event_index_submap, x, y);
        }
    }
    return false;
}

bool SubScene::isBuilding(int x, int y)
{
    return submap_info_->Building(x, y) > 0 && submap_info_->Building(x, y) < 9999;
    //if (current_submap_->Building(x, y) >= -2 && current_submap_->Building(x, y) <= 0)
    //{
    //    return false;
    //}
    //else
    //{
    //    return true;
    //}
}

bool SubScene::isWater(int x, int y)
{
    int num = submap_info_->Earth(x, y) / 2;
    if (num >= 179 && num <= 181 || num == 261 || num == 511 || num >= 662 && num <= 665 || num == 674)
    {
        return true;
    }
    return false;
}

//是否是可通过的事件，好像只有硝石使用了
bool SubScene::isCanPassEvent1(int x, int y)
{
    auto e = submap_info_->Event(x, y);
    if (e && !e->CannotWalk && e->Event1 > 0)
    {
        return true;
    }
    return false;
}

bool SubScene::isCannotPassEvent(int x, int y)
{
    auto e = submap_info_->Event(x, y);
    if (e && (e->CannotWalk || isBuilding(x, y)))    //这里加上判断建筑可能是不对
    {
        return true;
    }
    return false;
}

//what is this?
bool SubScene::isFall(int x, int y)
{
    //if (abs(Save::getInstance()->m_SceneMapData[scene_id_].Data[4][x][y] -
    //Save::getInstance()->m_SceneMapData[scene_id_].Data[4][Cx][Cy] > 10))
    //{
    //    true;
    //}
    return false;
}

bool SubScene::isExit(int x, int y)
{
    if (chess_mod_->blockExit(submap_info_->ID)) { return false; }
    if (submap_info_->ExitX[0] == x && submap_info_->ExitY[0] == y || submap_info_->ExitX[1] == x && submap_info_->ExitY[1] == y || submap_info_->ExitX[2] == x && submap_info_->ExitY[2] == y)
    {
        setExit(true);
        //Save::getInstance()->InSubMap = 1;
        return true;
    }
    return false;
}

bool SubScene::isJumpSubScene(int x, int y)
{
    if (submap_info_->JumpSubMap >= 0 && man_x_ == submap_info_->JumpX && man_y_ == submap_info_->JumpY)
    {
        int x, y;
        auto new_submap = Save::getInstance()->getSubMapInfo(submap_info_->JumpSubMap);
        if (submap_info_->MainEntranceX1 != 0)
        {
            //若原场景在大地图上有正常入口，则设置人物位置为新场景入口位置
            x = new_submap->EntranceX;
            y = new_submap->EntranceY;
        }
        else
        {
            //若原场景无法从大地图上进入，则设置人物在跳转返回位置
            x = new_submap->JumpReturnX;
            y = new_submap->JumpReturnY;
        }
        forceJumpSubScene(submap_info_->JumpSubMap, x, y);
        return true;
    }
    return false;
}

bool SubScene::isOutScreen(int x, int y)
{
    return (abs(view_x_ - x) >= 2 * view_width_region_ || abs(view_y_ - y) >= view_sum_region_);
}

bool SubScene::canWalk(int x, int y)
{
    bool ret = true;
    if (isOutLine(x, y) || isBuilding(x, y) || isWater(x, y) || isCannotPassEvent(x, y) || isFall(x, y))
    {
        ret = false;
    }
    //if (isCanPassEvent(x, y))
    //{
    //    ret = true;
    //}
    return ret;
}

void SubScene::forceExit()
{
    setVisible(false);
    setExit(true);
}

void SubScene::forceJumpSubScene(int submap_id, int x, int y)
{
    setID(submap_id);
    setManViewPosition(x, y);
    reDrawEarthTexture();
}

void SubScene::fillEarth()
{
    for (int i1 = 0; i1 < COORD_COUNT; i1++)
    {
        for (int i2 = 0; i2 < COORD_COUNT; i2++)
        {
            if (submap_info_->Earth(i1, i2) <= 0)
            {
                submap_info_->Earth(i1, i2) = 70;
            }
        }
    }
}

void SubScene::reDrawEarthTexture()
{
    auto earth_texture = Engine::getInstance()->getTexture("searth");
    if (earth_texture == nullptr)
    {
        return;
    }
    Engine::getInstance()->setRenderTarget(earth_texture);
    Engine::getInstance()->fillColor({ 0, 0, 0, 255 }, 0, 0, COORD_COUNT * TILE_W * 2, COORD_COUNT * TILE_H * 2);

    if (TextureManager::getInstance()->getTextureGroup("smap-earth")->getTextureCount() > 0)
    {
        TextureManager::getInstance()->renderTexture("smap-earth", submap_info_->ID, 0, 0);
    }
    else
    {
        //二者之差是屏幕中心与大纹理的中心的距离
        for (int i1 = 0; i1 < COORD_COUNT; i1++)
        {
            for (int i2 = 0; i2 < COORD_COUNT; i2++)
            {
                auto p = getPositionOnWholeEarth(i1, i2);
                int h = submap_info_->BuildingHeight(i1, i2);
                int num = submap_info_->Earth(i1, i2) / 2;
                //无高度地面
                if (num > 0 && h <= 2)
                {
                    TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y);
                }
            }
        }
    }
    Engine::getInstance()->resetRenderTarget();
}
