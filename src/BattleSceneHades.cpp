#include "BattleSceneHades.h"
#include "Audio.h"
#include "BattleRoleManager.h"
#include "ChessBalance.h"
#include "ChessCombo.h"
#include "ChessNeigong.h"
#include "ChessUiCommon.h"
#include "ChessEftIds.h"
#include "Engine.h"
#include "Event.h"
#include "Font.h"
#include "GameUtil.h"
#include "Head.h"
#include "ImGuiLayer.h"
#include "MainScene.h"
#include "TeamMenu.h"
#include "Weather.h"

#include <limits>

namespace
{
constexpr int PROJECTILE_SPEED = 7;
constexpr int PROJECTILE_BASE_TRAVEL = 100;  // travel distance at sd=1 (excluding spawn offset)
constexpr int PROJECTILE_TRAVEL_PER_SD = 25; // extra travel per sd
constexpr int HURT_FLASH_DURATION = 15;
constexpr int HURT_FLASH_PERIOD = 3;
constexpr int BLINK_SOUND_EFFECT_ID = 11;
constexpr int POST_SKILL_DASH_FRAMES = 30;
constexpr int STATUS_TEXT_SIZE = 12;
constexpr int EMPHASIS_TEXT_SIZE = 18;
constexpr int ULT_DAMAGE_TEXT_SIZE = 22;
constexpr int ROLE_STATUS_EFT_FRAMES = 24;
constexpr float ROLE_STATUS_EFT_Z_OFFSET = 42.0f;
constexpr double BLINK_WEAK_TARGET_DEF_WEIGHT = 4.0;
constexpr int BATTLE_TILE_W = 18;
constexpr int BATTLE_COORD_COUNT = BATTLEMAP_COORD_COUNT;

Pointf battlePos45To90(int x, int y)
{
    return {
        static_cast<float>(-y * BATTLE_TILE_W + x * BATTLE_TILE_W + BATTLE_COORD_COUNT * BATTLE_TILE_W),
        static_cast<float>(y * BATTLE_TILE_W + x * BATTLE_TILE_W),
        0.0f,
    };
}

Point battlePos90To45(double x, double y)
{
    x -= BATTLE_COORD_COUNT * BATTLE_TILE_W;
    return {
        static_cast<int>(std::round((x / BATTLE_TILE_W + y / BATTLE_TILE_W) / 2.0)),
        static_cast<int>(std::round((-x / BATTLE_TILE_W + y / BATTLE_TILE_W) / 2.0)),
        0,
    };
}

int calcProjectileReach(int select_distance, int spawn_offset)
{
    return spawn_offset + PROJECTILE_BASE_TRAVEL + (select_distance - 1) * PROJECTILE_TRAVEL_PER_SD;
}

int calcProjectileFrames(int select_distance, int spawn_offset)
{
    return (calcProjectileReach(select_distance, spawn_offset) - spawn_offset) / PROJECTILE_SPEED;
}

bool isSummonedCloneRole(const Role* r)
{
    if (!r) return false;
    auto& cs = KysChess::ChessCombo::getActiveStates();
    auto it = cs.find(r->ID);
    return it != cs.end() && it->second.isSummonedClone;
}

bool comboNameMatches(const std::string& comboName, std::initializer_list<const char*> candidates)
{
    auto* font = Font::getInstance();
    for (const char* candidate : candidates)
    {
        if (comboName == font->S2T(candidate))
            return true;
    }
    return false;
}

int getComboLookupId(const Role* r)
{
    if (!r) return -1;
    if (isSummonedCloneRole(r)) return -1;
    return r->RealID >= 0 ? r->RealID : r->ID;
}

BattleLogTone teamToBattleLogTone(int team)
{
    if (team == 0) return BattleLogTone::Ally;
    if (team == 1) return BattleLogTone::Enemy;
    return BattleLogTone::Neutral;
}

BattleLogFieldTone teamToFieldTone(int team)
{
    if (team == 0) return BattleLogFieldTone::AllyName;
    if (team == 1) return BattleLogFieldTone::EnemyName;
    return BattleLogFieldTone::Default;
}

void appendBattleLogSegment(BattleLogLine& line, std::string text, BattleLogFieldTone tone = BattleLogFieldTone::Default)
{
    if (text.empty())
    {
        return;
    }
    line.segments.push_back({std::move(text), tone});
}

bool roleHasCombo(const Role* r, std::initializer_list<const char*> comboNames)
{
    int comboLookupId = getComboLookupId(r);
    if (comboLookupId < 0)
        return false;

    auto combos = KysChess::ChessCombo::getCombosForRole(comboLookupId);
    auto& allCombos = KysChess::ChessCombo::getAllCombos();
    for (auto comboId : combos)
    {
        int comboIndex = static_cast<int>(comboId);
        if (comboIndex < 0 || comboIndex >= static_cast<int>(allCombos.size()))
            continue;
        if (comboNameMatches(allCombos[comboIndex].name, comboNames))
            return true;
    }
    return false;
}

bool rolesShareCombo(const Role* a, const Role* b, std::initializer_list<const char*> comboNames)
{
    return roleHasCombo(a, comboNames) && roleHasCombo(b, comboNames);
}

double calcBlinkReach(const Magic* magic)
{
    if (!magic) return BATTLE_TILE_W * 3.0;
    if (magic->AttackAreaType == 3) return 180.0;
    if (magic->AttackAreaType == 1 || magic->AttackAreaType == 2)
        return std::min(240.0, static_cast<double>(calcProjectileReach(magic->SelectDistance, BATTLE_TILE_W * 2) - 10));
    return std::max(BATTLE_TILE_W * 3.0, static_cast<double>(magic->SelectDistance * BATTLE_TILE_W));
}

bool isWithinGridRadius(const Role* center, const Role* target, int xRadius, int yRadius)
{
    if (!center || !target) return false;
    if (xRadius < 0 || yRadius < 0) return false;
    return std::abs(center->X() - target->X()) <= xRadius
        && std::abs(center->Y() - target->Y()) <= yRadius;
}

bool isWithinGridArea(const Role* center, const Role* target, int width, int height)
{
    if (width <= 0 || height <= 0) return false;
    return isWithinGridRadius(center, target, (width - 1) / 2, (height - 1) / 2);
}

bool hasMPBlock(const Role* r)
{
    auto& cs = KysChess::ChessCombo::getActiveStates();
    auto it = cs.find(r->ID);
    return it != cs.end() && it->second.mpBlockTimer > 0;
}

bool hasScriptedImpact(const BattleSceneAct::AttackEffect& ae)
{
    return ae.ScriptedDamage > 0 || ae.ScriptedStunFrames > 0;
}

bool attackHasExecuteEffect(const BattleSceneAct::AttackEffect& ae)
{
    if (!ae.Attacker)
        return false;

    auto& cs = KysChess::ChessCombo::getActiveStates();
    auto it = cs.find(ae.Attacker->ID);
    if (it == cs.end())
        return false;

    for (auto& effect : it->second.triggeredEffects)
    {
        if (effect.trigger == KysChess::Trigger::OnHit
            && effect.type == KysChess::EffectType::Execute
            && effect.triggerValue > 0
            && effect.value > 0)
        {
            return true;
        }
    }
    return false;
}

void changeRoleMP(Role* r, double delta)
{
    if (!r) return;
    if (delta > 0 && hasMPBlock(r)) return;

    if (delta > 0)
    {
        auto& cs = KysChess::ChessCombo::getActiveStates();
        auto it = cs.find(r->ID);
        if (it != cs.end() && it->second.mpRecoveryBonusPct > 0)
            delta *= (1.0 + it->second.mpRecoveryBonusPct / 100.0);
    }

    r->MP += delta;
}

void increaseCooldown(Role* r, int pct)
{
    if (!r || pct <= 0 || r->CoolDown <= 0) return;
    int increased = (r->CoolDown * (100 + pct) + 99) / 100;
    r->CoolDown = std::max(r->CoolDown + 1, increased);
}

// Apply freeze frames to a role, accounting for low-HP immunity and freeze shield
void applyFrozen(Role* r, int frames)
{
    if (frames <= 0) return;
    // Low HP immunity: below 25% HP
    if (r->MaxHP > 0 && r->HP < r->MaxHP * 0.25) return;
    // Freeze shield absorption
    auto& cs = KysChess::ChessCombo::getMutableStates();
    auto it = cs.find(r->ID);
    if (it != cs.end() && it->second.controlImmunityFrames > 0)
    {
        int absorbed = std::min(frames, it->second.controlImmunityFrames);
        it->second.controlImmunityFrames -= absorbed;
        frames -= absorbed;
    }
    if (frames > 0)
    {
        r->Frozen += frames;
        r->FrozenMax = r->Frozen;  // Reset max when frozen is applied
    }
}
}    // namespace

int BattleSceneHades::getOperationType(int attackAreaType)
{
    if (attackAreaType == 0) return 0;
    if (attackAreaType == 1 || attackAreaType == 2) return 2;
    if (attackAreaType == 3) return 1;
    return -1;
}

const char* BattleSceneHades::getOperationTypeName(int operationType)
{
    if (operationType == 0) return "輕擊";
    if (operationType == 1) return "重擊";
    if (operationType == 2) return "遠程";
    return "未知";
}

BattleSceneHades::BattleSceneHades(KysChess::ChessRoleSave& roleSave, KysChess::ChessProgress& progress, KysChess::ChessManager& chessManager)
    : roleSave_(roleSave), progress_(progress), chessManager_(chessManager)
{
    full_window_ = 1;
    COORD_COUNT = BATTLEMAP_COORD_COUNT;

    earth_layer_.resize(COORD_COUNT);
    building_layer_.resize(COORD_COUNT);

    // heads_.resize(1);
    // int i = 0;
    // for (auto& h : heads_)
    // {
    //     h = std::make_shared<Head>();
    //     h->setAlwaysLight(1);
    //     addChild(h, 10, 10 + (i++) * 80);
    //     h->setVisible(false);
    // }
    // heads_[0]->setVisible(true);
    // heads_[0]->setRole(Save::getInstance()->getRole(0));

    head_boss_.resize(6);
    for (auto& h : head_boss_)
    {
        h = std::make_shared<Head>();
        h->setStyle(1);
        h->setVisible(false);
        addChild(h);
    }
    makeSpecialMagicEffect();
}

BattleSceneHades::BattleSceneHades(int id, KysChess::ChessRoleSave& roleSave, KysChess::ChessProgress& progress, KysChess::ChessManager& chessManager) :
    BattleSceneHades(roleSave, progress, chessManager)
{
    setID(id);
}

BattleSceneHades::~BattleSceneHades()
{
}

Color BattleSceneHades::calculateHurtFlashColor(const Role* r, const Color& base_color) const
{
    auto it = hurt_flash_timers_.find(r->ID);
    if (it == hurt_flash_timers_.end()) return base_color;

    int timer = it->second;
    int phase = timer % HURT_FLASH_PERIOD;

    if (phase < 2)
    {
        return {255, 100, 100, base_color.a};
    }
    return base_color;
}

void BattleSceneHades::addFloatingText(Role* role, const std::string& text, Color color, int size, int type)
{
    TextEffect effect;
    effect.set(text, color, role);
    effect.Size = size;
    effect.Type = type;
    text_effects_.push_back(std::move(effect));
}

void BattleSceneHades::addRoleEffect(Role* role, int eftId, int totalFrames)
{
    if (!role)
        return;

    AttackEffect effect;
    effect.FollowRole = role;
    effect.Pos = {0.0f, 0.0f, ROLE_STATUS_EFT_Z_OFFSET};
    effect.setEft(eftId);
    effect.TotalFrame = totalFrames > 0
        ? std::max(totalFrames, effect.TotalEffectFrame)
        : std::max(1, effect.TotalEffectFrame);
    effect.Frame = 0;
    effect.NoHurt = 1;
    effect.IsMain = 0;
    effect.IgnoreProjectileCancel = 1;
    attack_effects_.push_back(std::move(effect));
}

void BattleSceneHades::draw()
{
    constexpr int kCullMargin = 96;

    //在这个模式下，使用的是直角坐标
    Engine::getInstance()->setRenderTarget("scene");
    Engine::getInstance()->fillColor({ 0, 0, 0, 255 }, 0, 0, render_center_x_ * 2, render_center_y_ * 2);

    //以下是计算出需要画的区域，先画到一个大图上，再转贴到窗口
    {
        auto p = pos90To45(pos_.x, pos_.y);
        man_x_ = p.x;
        man_y_ = p.y;
    }

    //整个场景
    auto whole_scene = Engine::getInstance()->getTexture("whole_scene");
    Engine::getInstance()->setRenderTarget(whole_scene);
    int w = render_center_x_ * 2;
    int h = render_center_y_ * 2;
    Rect camera_rect = { int(pos_.x - render_center_x_ - x_), int(pos_.y / 2 - render_center_y_ - y_), w, h };
    Rect clear_rect = {
        camera_rect.x - kCullMargin,
        camera_rect.y - kCullMargin,
        camera_rect.w + kCullMargin * 2,
        camera_rect.h + kCullMargin * 2
    };
    clear_rect.x = std::max(0, clear_rect.x);
    clear_rect.y = std::max(0, clear_rect.y);
    clear_rect.w = std::min(clear_rect.w, COORD_COUNT * TILE_W * 2 - clear_rect.x);
    clear_rect.h = std::min(clear_rect.h, COORD_COUNT * TILE_H * 2 - clear_rect.y);
    if (clear_rect.w > 0 && clear_rect.h > 0)
    {
        Engine::getInstance()->fillColor({ 0, 0, 0, 255 }, clear_rect.x, clear_rect.y, clear_rect.w, clear_rect.h);
    }

    auto is_visible_world = [&](double world_x, double world_y) -> bool
    {
        return world_x >= clear_rect.x && world_x <= clear_rect.x + clear_rect.w
            && world_y >= clear_rect.y && world_y <= clear_rect.y + clear_rect.h;
    };

    auto earth_tex = Engine::getInstance()->getTexture("earth");
    if (earth_tex)
    {
        //此画法节省资源
        Color c = { 255, 255, 255, 255 };
        Engine::getInstance()->setColor(earth_tex, c);
        auto p = getPositionOnWholeEarth(man_x_, man_y_);
        Rect rect0 = { int(pos_.x - render_center_x_ - x_), int(pos_.y / 2 - render_center_y_ - y_), w, h };
        Rect rect1 = rect0;
        //注意这种画法，地面最上面会缺一块
        rect0.y += TILE_H * 2;
        if (rect0.x < 0)
        {
            rect1.x -= rect0.x;
            rect0.x = 0;
        }
        if (rect0.y < 0)
        {
            rect1.y -= rect0.y;
            rect0.y = 0;
        }
        if (rect0.x + rect0.w > COORD_COUNT * TILE_W * 2)
        {
            rect1.w -= (rect0.x + rect0.w - COORD_COUNT * TILE_W * 2);
            rect0.w = COORD_COUNT * TILE_W * 2 - rect0.x;
        }
        if (rect0.y + rect0.h > COORD_COUNT * TILE_H * 2)
        {
            rect1.h -= (rect0.y + rect0.h - COORD_COUNT * TILE_H * 2);
            rect0.h = COORD_COUNT * TILE_H * 2 - rect0.y;
        }
        Engine::getInstance()->renderTexture(earth_tex, &rect0, &rect1);
    }
    else
    {
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
                    Color color = { 255, 255, 255, 255 };
                    bool need_draw = true;
                    if (need_draw && num > 0)
                    {
                        TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y / 2, color);
                    }
                }
            }
        }
    }
    struct DrawInfo
    {
        TextureWarpper* tex = nullptr;
        Pointf p;
        Pointf sort_p;
        Color color{ 255, 255, 255, 255 };
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
                    info.tex = TextureManager::getInstance()->getTexture("smap", num);
                    if (!info.tex)
                    {
                        continue;
                    }
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
        if (!is_visible_world(r->Pos.x, r->Pos.y / 2.0))
        {
            continue;
        }
        //if (r->Dead) { continue; }
        DrawInfo info;
        auto path = std::format("fight/fight{:03}", r->HeadID);
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
        info.tex = TextureManager::getInstance()->getTexture(path, calRolePic(r, r->ActType, r->ActFrame));
        if (!info.tex)
        {
            continue;
        }
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

        // 应用受击闪红
        info.color = calculateHurtFlashColor(r, info.color);

        info.shadow = 1;
        //TextureManager::getInstance()->renderTexture(path, pic, r->X1, r->Y1, color, alpha);
        draw_infos.emplace_back(std::move(info));
    }

    //effects
    for (auto& ae : attack_effects_)
    {
        //for (auto r : ae.Defender)
        {
            auto effect_pos = ae.FollowRole ? ae.FollowRole->Pos + ae.Pos : ae.Pos;
            if (!is_visible_world(effect_pos.x, effect_pos.y / 2.0))
            {
                continue;
            }
            DrawInfo info;
            int frame = 0;
            if (ae.TotalEffectFrame > 0)
            {
                frame = ae.Frame % ae.TotalEffectFrame;
            }
            info.tex = TextureManager::getInstance()->getTexture(ae.Path, frame);
            if (!info.tex)
            {
                continue;
            }
            info.p = effect_pos;
            info.sort_p = effect_pos;
            info.sort_p.y += 10000;  // force projectiles to render on top
            info.color = { 255, 255, 255, 255 };
            info.alpha = ae.FollowRole ? 255 : 192;
            info.shadow = ae.FollowRole ? 0 : 1;
            if (!ae.FollowRole && ae.Attacker && ae.Attacker->Team == 0)
            {
                info.shadow = 2;
            }
            if (!ae.FollowRole)
                info.alpha = 255 * (ae.TotalFrame * 1.25 - ae.Frame) / (ae.TotalFrame * 1.25);    //越来越透明
            draw_infos.emplace_back(std::move(info));
            //TextureManager::getInstance()->renderTexture(ae.Path, ae.Frame % ae.TotalEffectFrame, ae.X1, ae.Y1 / 2, { 255, 255, 255, 255 }, 192);
        }
    }

    auto sort_building = [](DrawInfo& d1, DrawInfo& d2)
    {
        auto& p1 = d1.sort_p.y != 0 ? d1.sort_p : d1.p;
        auto& p2 = d2.sort_p.y != 0 ? d2.sort_p : d2.p;
        if (p1.y != p2.y)
        {
            return p1.y < p2.y;
        }
        else
        {
            return p1.x < p2.x;
        }
    };
    std::sort(draw_infos.begin(), draw_infos.end(), sort_building);

    //影子
    for (auto& d : draw_infos)
    {
        if (d.shadow)
        {
            d.tex->load();
            if (d.tex->getTexture())
            {
                double scalex = 1, scaley = 0.3;
                int yd = d.tex->dy * 0.7;
                if (d.rot)
                {
                    scalex = 0.3;
                    scaley = 1;
                    yd = d.tex->dy * 0.1;
                }
                if (d.shadow == 1)
                {
                    TextureManager::getInstance()->renderTexture(d.tex, d.p.x, d.p.y / 2 + yd, { 32, 32, 32, 255 }, d.alpha / 2, scalex, scaley, d.rot);
                }
                if (d.shadow == 2)
                {
                    TextureManager::getInstance()->renderTexture(d.tex, d.p.x, d.p.y / 2 + yd, { 128, 128, 128, 255 }, d.alpha / 2, scalex, scaley, d.rot, 128);
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
        TextureManager::getInstance()->renderTexture(d.tex, d.p.x, d.p.y / 2 - d.p.z, d.color, d.alpha, scaley, 1, d.rot, d.white);
    }

    for (auto r : battle_roles_)
    {
        if (is_visible_world(r->Pos.x, r->Pos.y / 2.0))
        {
            renderExtraRoleInfo(r, r->Pos.x, r->Pos.y / 2);
        }
    }

    if (swapSelected_ && is_visible_world(swapSelected_->Pos.x, swapSelected_->Pos.y / 2.0))
    {
        Engine::getInstance()->fillColor({ 255, 255, 0, 80 }, swapSelected_->Pos.x - 15, swapSelected_->Pos.y / 2 - 5, 30, 10);
    }

    Color c = { 255, 255, 255, 255 };
    Engine::getInstance()->setColor(whole_scene, c);
    //获取的是中心位置，如贴图应减掉屏幕尺寸的一半
    Rect rect0 = { int(pos_.x - render_center_x_ - x_), int(pos_.y / 2 - render_center_y_ - y_), w, h };
    Rect rect1 = { 0, 0, w, h };
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
    for (auto& te : text_effects_)
    {
        if (is_visible_world(te.Pos.x, te.Pos.y / 2.0))
        {
            Font::getInstance()->draw(te.Text, te.Size, te.Pos.x, te.Pos.y / 2, te.color, 255);
        }
    }

    Engine::getInstance()->setRenderTarget("scene");
    if (close_up_)
    {
        rect0.w /= 2;
        rect0.h /= 2;
        rect0.x += rect0.w / 2;
        rect0.y += rect0.h / 2 - 20;
    }

    Engine::getInstance()->renderTexture(whole_scene, &rect0, &rect1, 0);

    Engine::getInstance()->renderTextureToMain("scene");

    // Draw total frames elapsed on top left
    Font::getInstance()->draw(std::to_string(current_frame_), 20, 10, 10, { 255, 255, 255, 255 }, 200);

    //if (result_ >= 0)
    //{
    //    Engine::getInstance()->fillColor({ 0, 0, 0, 128 }, 0, 0, -1, -1);
    //}
}

void BattleSceneHades::dealEvent(EngineEvent& e)
{
    auto engine = Engine::getInstance();
    if (shake_ > 0)
    {
        x_ = rand_.rand_int(3) - rand_.rand_int(3);
        y_ = rand_.rand_int(3) - rand_.rand_int(3);
        shake_--;
    }
    if (frozen_ > 0)
    {
        frozen_--;
        engine->gameControllerRumble(100, 100, 50);
        return;
    }
    decreaseToZero(close_up_);
    if (close_up_ == 0)
    {
        for (auto r1 : battle_roles_)
        {
            if (r1->Team == 0 && r1->Dead == 0)
            {
                pos_ = r1->Pos;
            }
        }
    }

    backRun1();
}

void BattleSceneHades::dealEvent2(EngineEvent& e)
{
}

void BattleSceneHades::onEntrance()
{
    calViewRegion();

    //注意此时才能得到窗口的大小，用来设置头像的位置
    head_self_->setPosition(10, 10);
    int count = 0;
    for (auto& h : head_boss_)
    {
        h->setPosition(Engine::getInstance()->getUIWidth() / 2 - h->getWidth() / 2, Engine::getInstance()->getUIHeight() - 50 - (25 * count++));
    }
    addChild(Weather::getInstance());

    //makeEarthTexture(); //注意高度稍微多了一点

    //此处创建了一个大的纹理，用于渲染整个场景
    Engine::getInstance()->createRenderedTexture("whole_scene", COORD_COUNT * TILE_W * 2, COORD_COUNT * TILE_H * 2);

    //首先设置位置和阵营，其他的后面统一处理
    int battleUid = 10000;
    int cloneSummonCount = 0;
    std::vector<size_t> cloneSourceIndices;
    //敌方
    for (int i = 0; i < BATTLE_ENEMY_COUNT; i++)
    {
        auto r_ptr = roleSave_.getRole(info_->Enemy[i]);
        if (r_ptr)
        {
            enemies_obj_.push_back(*r_ptr);
            auto r = &enemies_obj_.back();
            r->ID = battleUid++;
            r->resetBattleInfo();
            r->RealID = r_ptr->ID;
            enemies_.push_back(r);
            LOG("Adding enemy battle ID {} with {} name {}\n", r->ID, r->RealID, r->Name);
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

    for (int i = 0; i < head_boss_.size() && i < enemies_.size(); i++)
    {
        head_boss_[i]->setRole(enemies_[enemies_.size() - i - 1]);
    }
    if (!extended_teammates_.empty())
    {
        while (!enemies_.empty())
        {
            battle_roles_.push_back(enemies_.front());
            enemies_.pop_front();
        }
    }
    else
    {
        for (int i = 0; i < 6; i++)
        {
            if (!enemies_.empty())
            {
                battle_roles_.push_back(enemies_.front());
                enemies_.pop_front();
            }
        }
    }

    //判断是不是有自动战斗人物
    if (!extended_teammates_.empty())
    {
        for (const auto& teammate_info : extended_teammates_)
        {
            auto r_ptr = roleSave_.getRole(teammate_info.ID);
            if (r_ptr)
            {
                friends_obj_.push_back(*r_ptr);
                auto r = &friends_obj_.back();
                r->ID = battleUid++;
                r->resetBattleInfo();
                r->RealID = r_ptr->ID;
                friends_.push_back(r);
                r->Auto = 2;
            }
        }

        // Apply star + combo enhancements on battle copies
        for (size_t i = 0; i < friends_obj_.size() && i < extended_teammates_.size(); i++)
            KysChess::BattleRoleManager::applyStarBonus(&friends_obj_[i], extended_teammates_[i].star);
        for (size_t i = 0; i < enemies_obj_.size() && i < enemy_stars_.size(); i++)
            KysChess::BattleRoleManager::applyStarBonus(&enemies_obj_[i], enemy_stars_[i]);

        // Detect combos using RealID (original Save IDs, deduped by set)
        auto allyChessVec = [&]() {
            std::vector<KysChess::Chess> v;
            for (size_t i = 0; i < friends_obj_.size(); i++)
            {
                auto orig = roleSave_.getRole(friends_obj_[i].RealID);
                if (orig) v.push_back({orig, i < extended_teammates_.size() ? extended_teammates_[i].star : 1});
            }
            return v;
        }();
        auto enemyChessVec = [&]() {
            std::vector<KysChess::Chess> v;
            for (size_t i = 0; i < enemies_obj_.size(); i++)
            {
                auto orig = roleSave_.getRole(enemies_obj_[i].RealID);
                if (orig) v.push_back({orig, i < enemy_stars_.size() ? enemy_stars_[i] : 0});
            }
            return v;
        }();
        auto allyCombos = KysChess::ChessCombo::detectCombos(allyChessVec);
        auto enemyCombos = KysChess::ChessCombo::detectCombos(enemyChessVec);
        auto allyStates = KysChess::ChessCombo::buildComboStates(allyCombos);
        auto enemyStates = KysChess::ChessCombo::buildComboStates(enemyCombos);

        // Merge neigong global effects into ally states
        {
            auto& obtained = progress_.getObtainedNeigong();
            if (!obtained.empty())
            {
                auto& pool = KysChess::ChessNeigong::getPool();
                std::vector<KysChess::ComboEffect> globalEffects;
                for (int mid : obtained)
                    for (auto& ng : pool)
                        if (ng.magicId == mid)
                        { globalEffects.insert(globalEffects.end(), ng.effects.begin(), ng.effects.end()); break; }
                std::vector<int> allyIds;
                for (auto& c : allyChessVec) allyIds.push_back(c.role->ID);
                KysChess::ChessBattleEffects::mergeEffects(allyStates, globalEffects, allyIds);
            }
        }

        int enemyTopDebuffCount = 0;
        int enemyTopDebuffValue = 0;
        for (auto& entry : allyStates)
        {
            auto& state = entry.second;
            enemyTopDebuffCount = std::max(enemyTopDebuffCount, state.enemyTopDebuffCount);
            enemyTopDebuffValue = std::max(enemyTopDebuffValue, state.enemyTopDebuffValue);
        }
        if (enemyTopDebuffCount > 0 && enemyTopDebuffValue > 0 && !enemies_obj_.empty())
        {
            std::vector<size_t> enemyOrder;
            enemyOrder.reserve(enemies_obj_.size());
            for (size_t i = 0; i < enemies_obj_.size(); ++i)
                enemyOrder.push_back(i);

            std::stable_sort(enemyOrder.begin(), enemyOrder.end(), [&](size_t l, size_t r) {
                int lStar = l < enemy_stars_.size() ? enemy_stars_[l] : 0;
                int rStar = r < enemy_stars_.size() ? enemy_stars_[r] : 0;
                return lStar > rStar;
            });

            for (int i = 0; i < enemyTopDebuffCount && i < static_cast<int>(enemyOrder.size()); ++i)
            {
                auto& enemy = enemies_obj_[enemyOrder[i]];
                enemy.Attack = std::max(0, enemy.Attack - enemyTopDebuffValue);
                enemy.Defence = std::max(0, enemy.Defence - enemyTopDebuffValue);
            }
        }

        // Apply combo stat buffs on copies and remap to battle IDs
        KysChess::ChessCombo::clearActiveStates();
        auto& cs = KysChess::ChessCombo::getMutableStates();
        auto applyStateToCopy = [&](Role& r, KysChess::RoleComboState& s) {
            r.MaxHP += s.flatHP;
            r.Attack += s.flatATK;
            r.Defence += s.flatDEF;
            r.Speed += s.flatSPD;
            if (s.pctHP != 0) r.MaxHP = static_cast<int>(r.MaxHP * (1.0 + s.pctHP / 100.0));
            if (s.pctATK != 0) r.Attack = static_cast<int>(r.Attack * (1.0 + s.pctATK / 100.0));
            if (s.pctDEF != 0) r.Defence = static_cast<int>(r.Defence * (1.0 + s.pctDEF / 100.0));
            r.HP = r.MaxHP;
            if (s.shieldPctMaxHP > 0)
                s.shield = r.MaxHP * s.shieldPctMaxHP / 100;
        };
        auto applyEquipmentEffects = [&](KysChess::RoleComboState& state, int weaponId, int armorId) {
            auto applyById = [&](int itemId) {
                if (itemId < 0) return;
                auto* eq = KysChess::ChessEquipment::getById(itemId);
                if (!eq) return;
                for (auto& effect : eq->effects)
                    KysChess::ChessBattleEffects::applyEffect(state, effect);
            };
            applyById(weaponId);
            applyById(armorId);
        };
        auto initializeTimedEffects = [](KysChess::RoleComboState& state) {
            if (state.damageImmunityAfterFrames > 0)
                state.damageImmunityTimer = state.damageImmunityAfterFrames;
            if (state.autoUltimateAfterFrames > 0)
                state.autoUltimateTimer = state.autoUltimateAfterFrames;
        };
        auto applyOnCopies = [&](std::deque<Role>& objs, std::map<int, KysChess::RoleComboState>& states, auto equipmentLookup) {
            for (size_t index = 0; index < objs.size(); ++index)
            {
                auto& r = objs[index];
                KysChess::RoleComboState battleState;
                auto it = states.find(r.RealID);
                if (it != states.end())
                    battleState = it->second;

                auto [weaponId, armorId] = equipmentLookup(index);
                applyEquipmentEffects(battleState, weaponId, armorId);
                applyStateToCopy(r, battleState);
                initializeTimedEffects(battleState);
                battleState.blockFirstHitsRemaining = battleState.blockFirstHitsCount;
                cs[r.ID] = battleState;
            }
        };
        applyOnCopies(friends_obj_, allyStates, [&](size_t index) {
            int weaponId = index < teammate_weapons_.size() ? teammate_weapons_[index] : -1;
            int armorId = index < teammate_armors_.size() ? teammate_armors_[index] : -1;
            if (weaponId >= 0 || armorId >= 0)
                return std::pair{weaponId, armorId};
            if (index >= extended_teammates_.size()) return std::pair{-1, -1};

            auto& teammate = extended_teammates_[index];
            if (teammate.weaponId >= 0 || teammate.armorId >= 0)
                return std::pair{teammate.weaponId, teammate.armorId};

            KysChess::ChessInstanceID chessInstanceId{teammate.chessInstanceId};
            auto chess = chessManager_.tryFindChessByInstanceId(chessInstanceId);
            if (!chess)
                return std::pair{-1, -1};

            auto& c = *chess;
            return std::pair{
                c.weaponInstance.itemId,
                c.armorInstance.itemId
            };
        });
        applyOnCopies(enemies_obj_, enemyStates, [&](size_t index) {
            int weaponId = index < enemy_weapons_.size() ? enemy_weapons_[index] : -1;
            int armorId = index < enemy_armors_.size() ? enemy_armors_[index] : -1;
            return std::pair{weaponId, armorId};
        });

        for (auto& role : friends_obj_)
        {
            auto it = cs.find(role.ID);
            if (it != cs.end())
                cloneSummonCount = std::max(cloneSummonCount, it->second.cloneSummonCount);
        }

        if (cloneSummonCount > 0)
        {
            struct CloneSourceCandidate
            {
                size_t index = static_cast<size_t>(-1);
                int chessInstanceId = -1;
                int star = 0;
                int power = 0;
            };

            std::vector<CloneSourceCandidate> cloneCandidates;
            for (size_t i = 0; i < friends_obj_.size() && i < extended_teammates_.size(); ++i)
            {
                if (!roleHasCombo(&friends_obj_[i], {"真武七截阵"}))
                    continue;

                cloneCandidates.push_back({
                    i,
                    extended_teammates_[i].chessInstanceId,
                    extended_teammates_[i].star,
                    friends_obj_[i].MaxHP + friends_obj_[i].Attack + friends_obj_[i].Defence});
            }

            std::sort(cloneCandidates.begin(), cloneCandidates.end(), [](const auto& left, const auto& right) {
                if (left.star != right.star) return left.star > right.star;
                if (left.power != right.power) return left.power > right.power;
                return left.index < right.index;
            });

            std::set<int> usedInstanceIds;
            std::vector<size_t> fallbackIndices;
            for (const auto& candidate : cloneCandidates)
            {
                if (candidate.chessInstanceId >= 0)
                {
                    if (!usedInstanceIds.insert(candidate.chessInstanceId).second)
                    {
                        fallbackIndices.push_back(candidate.index);
                        continue;
                    }
                }
                cloneSourceIndices.push_back(candidate.index);
            }
            cloneSourceIndices.insert(cloneSourceIndices.end(), fallbackIndices.begin(), fallbackIndices.end());
            if (cloneSummonCount < static_cast<int>(cloneSourceIndices.size()))
            {
                cloneSourceIndices.resize(cloneSummonCount);
            }
        }
    }
    else if (info_->AutoTeamMate[0] >= 0)
    {
        for (int i = 0; i < TEAMMATE_COUNT; i++)
        {
            auto r = roleSave_.getRole(info_->AutoTeamMate[i]);
            if (r)
            {
                friends_.push_back(r);
                r->Auto = 2;    //由AI控制
            }
        }
    }

    if (extended_teammates_.empty() && 1)    //准许队友出场
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
    role_ = nullptr;
    for (int i = 0; i < friends_.size(); i++)
    {
        auto r = friends_[i];
        if (r)
        {
            battle_roles_.push_back(r);
            // Use extended teammate positions if available, otherwise use BattleInfo positions
            if (!extended_teammates_.empty() && i < extended_teammates_.size())
            {
                r->setPositionOnly(extended_teammates_[i].X, extended_teammates_[i].Y);
            }
            else
            {
                r->setPositionOnly(info_->TeamMateX[i], info_->TeamMateY[i]);
            }
            r->Team = 0;
            setRoleInitState(r);
        }
    }

    if (!extended_teammates_.empty()
        && cloneSummonCount > 0
        && !cloneSourceIndices.empty()
        && !clone_spawn_positions_.empty())
    {
        auto isOccupied45 = [&](int x, int y) {
            for (auto role : battle_roles_)
            {
                if (role->Dead != 0) continue;
                auto pos = pos90To45(role->Pos.x, role->Pos.y);
                if (pos.x == x && pos.y == y)
                    return true;
            }
            return false;
        };

        auto& cs = KysChess::ChessCombo::getMutableStates();
        int spawned = 0;
        for (auto [x, y] : clone_spawn_positions_)
        {
            if (spawned >= cloneSummonCount) break;
            if (!canWalk45(x, y) || isOccupied45(x, y)) continue;

            size_t sourceIndex = cloneSourceIndices[spawned % cloneSourceIndices.size()];
            if (sourceIndex >= friends_obj_.size())
                continue;

            auto source = &friends_obj_[sourceIndex];
            KysChess::RoleComboState cloneState;
            if (auto it = cs.find(source->ID); it != cs.end())
                cloneState = it->second;
            cloneState.isSummonedClone = true;
            cloneState.cloneSummonCount = 0;
            cloneState.postSkillDashPending = false;
            cloneState.postSkillDashTimer = 0;
            cloneState.forcePullProtectUsed = false;
            cloneState.forcePullExecuteUsed = false;
            cloneState.onSkillTeamHealPending = false;
            if (cloneState.damageImmunityAfterFrames > 0)
                cloneState.damageImmunityTimer = cloneState.damageImmunityAfterFrames;
            if (cloneState.autoUltimateAfterFrames > 0)
                cloneState.autoUltimateTimer = cloneState.autoUltimateAfterFrames;

            friends_obj_.push_back(*source);
            auto clone = &friends_obj_.back();
            clone->ID = battleUid++;
            clone->Auto = 2;
            clone->Team = source->Team;
            clone->Dead = 0;
            clone->LastAttacker = nullptr;
            clone->Velocity = {0, 0, 0};
            clone->Acceleration = {0, 0, 0};
            clone->UsingMagic = nullptr;
            clone->UsingItem = nullptr;
            clone->HaveAction = 0;
            clone->ActFrame = 0;
            clone->CoolDown = 0;
            clone->Frozen = 0;
            clone->FrozenMax = 0;
            clone->Invincible = 0;
            clone->HurtFrame = 0;
            clone->HurtThisFrame = 0;
            clone->Shake = 0;
            clone->FindingWay = 0;
            clone->OperationCount = 0;
            clone->setPositionOnly(x, y);
            setRoleInitState(clone);
            battle_roles_.push_back(clone);
            cs[clone->ID] = cloneState;
            spawned++;
        }
    }

    // Center camera on allies
    if (!friends_.empty())
    {
        int sx = 0, sy = 0;
        for (auto r : friends_) { sx += r->X(); sy += r->Y(); }
        pos_ = pos45To90(sx / friends_.size(), sy / friends_.size());
    }

    // Pre-battle position swap
    if (!extended_teammates_.empty() && progress_.isPositionSwapEnabled())
    {
        auto prompt = std::make_shared<MenuText>(std::vector<std::string>{ "地圖佈陣", "列表佈陣", "直接開戰" });
        prompt->setFontSize(36);
        prompt->arrange(0, 0, 0, 45);
        prompt->runAtPosition(300, 220);
        int choice = prompt->getResult();
        if (choice == 0)
        {
            runPositionSwapLoop();
        }
        else if (choice == 1)
        {
            runListBasedSwap();
        }
    }

    // int i = 0;
    // for (auto r : friends_)
    // {
    //     if (r && r != heads_[0]->getRole())
    //     {
    //         auto head = std::make_shared<Head>();
    //         head->setRole(r);
    //         head->setAlwaysLight(true);
    //         addChild(head, Engine::getInstance()->getWindowWidth() - 280, 10 + 80 * i++);
    //     }
    // }

    // // Hide main character head if using extended teammates (no main character)
    // if (!extended_teammates_.empty())
    // {
    //     heads_[0]->setVisible(false);
    // }

    Audio::getInstance()->playMusic(KysChess::getRandomBattleMusic());
    // Audio::getInstance()->playMusic(info_->Music);
}

void BattleSceneHades::onExit()
{
    hurt_flash_timers_.clear();
    execution_popup_roles_.clear();
    Engine::getInstance()->hideBattleLogWindow();
    battle_log_shown_ = false;
}

class PositionSwapNode : public RunNode
{
    BattleSceneHades* battle_;
public:
    PositionSwapNode(BattleSceneHades* b) : battle_(b) {}

    void dealEvent(EngineEvent& e) override
    {
        // Tile-click swap
        if (e.type == EVENT_MOUSE_BUTTON_UP && e.button.button == BUTTON_LEFT)
        {
            auto p = battle_->getMousePosition(battle_->man_x_, battle_->man_y_);
            Role* clicked = nullptr;
            for (auto r : battle_->getBattleRoles())
            {
                if (r && r->Team == 0 && r->X() == p.x && r->Y() == p.y)
                {
                    clicked = r;
                    break;
                }
            }
            if (!clicked) return;
            if (!battle_->swapSelected_)
            {
                battle_->swapSelected_ = clicked;
            }
            else if (clicked != battle_->swapSelected_)
            {
                int ax = battle_->swapSelected_->X(), ay = battle_->swapSelected_->Y();
                int bx = clicked->X(), by = clicked->Y();
                battle_->swapSelected_->setPositionOnly(bx, by);
                clicked->setPositionOnly(ax, ay);
                auto pa = battle_->pos45To90(bx, by);
                auto pb = battle_->pos45To90(ax, ay);
                battle_->swapSelected_->Pos.x = pa.x;
                battle_->swapSelected_->Pos.y = pa.y;
                clicked->Pos.x = pb.x;
                clicked->Pos.y = pb.y;
                battle_->swapSelected_ = nullptr;
            }
        }
    }

    void onPressedCancel() override
    {
        auto menu = std::make_shared<MenuText>(std::vector<std::string>{ "佈陣完成", "繼續調整" });
        menu->setFontSize(36);
        menu->arrange(0, 0, 0, 45);
        menu->runAtPosition(400, 300);
        if (menu->getResult() == 0)
        {
            battle_->swapSelected_ = nullptr;
            exit_ = true;
        }
    }
};

void BattleSceneHades::runPositionSwapLoop()
{
    swapSelected_ = nullptr;
    auto node = std::make_shared<PositionSwapNode>(this);
    node->run();
}

void BattleSceneHades::runListBasedSwap()
{
    positionSwapActive_ = true;
    // Collect friendly roles
    std::vector<Role*> allies;
    for (auto r : battle_roles_)
    {
        if (r && r->Team == 0)
            allies.push_back(r);
    }
    if (allies.size() < 2) return;

    // Build menu items with proper alignment using getTextDrawSize
    // Name is left-aligned, coordinates are right-aligned
    auto buildNames = [&](int highlight = -1) {
        // First pass: find max name width and max coord width separately
        int maxNameLen = 0;
        int maxCoordLen = 0;
        for (auto r : allies)
        {
            std::string name = r->Name;
            std::string coord = std::format("({},{})", r->X(), r->Y());
            int nameLen = Font::getTextDrawSize(name);
            int coordLen = Font::getTextDrawSize(coord);
            if (nameLen > maxNameLen) maxNameLen = nameLen;
            if (coordLen > maxCoordLen) maxCoordLen = coordLen;
        }
        std::vector<std::string> names;
        std::vector<Color> colors;
        std::vector<Color> outlineColors;
        std::vector<bool> animateOutlines;
        for (int i = 0; i < (int)allies.size(); i++)
        {
            std::string name = allies[i]->Name;
            std::string coord = std::format("({},{})", allies[i]->X(), allies[i]->Y());
            int nameLen = Font::getTextDrawSize(name);
            int coordLen = Font::getTextDrawSize(coord);
            // Pad name to max, then pad coord prefix so coords right-align
            std::string s = name;
            int gap = (maxNameLen - nameLen) + (maxCoordLen - coordLen) + 2;
            for (int g = 0; g < gap; g++) s += ' ';
            s += coord;
            names.push_back(s);
            colors.push_back({255, 255, 255, 255});
            if (i == highlight)
            {
                outlineColors.push_back({255, 255, 100, 255});
                animateOutlines.push_back(true);
            }
            else
            {
                outlineColors.push_back({0, 0, 0, 0});
                animateOutlines.push_back(false);
            }
        }
        return std::make_tuple(names, colors, outlineColors, animateOutlines);
    };

    while (true)
    {
        // Pick first role
        auto [names1, colors1, outlines1, anim1] = buildNames();
        auto menu1 = std::make_shared<MenuText>();
        menu1->setStrings(names1, colors1, outlines1, anim1);
        menu1->setFontSize(28);
        menu1->arrange(0, 0, 0, 36);
        menu1->runAtPosition(100, 100);
        int sel1 = menu1->getResult();
        if (sel1 < 0 || sel1 >= (int)allies.size())
        {
            // Cancelled first pick — ask to confirm or continue
            auto confirm = std::make_shared<MenuText>(std::vector<std::string>{ "佈陣完成", "繼續調整" });
            confirm->setFontSize(36);
            confirm->arrange(0, 0, 0, 45);
            confirm->runAtPosition(400, 300);
            if (confirm->getResult() == 0)
                break;
            continue;
        }

        // Pick second role — highlight the first selection
        auto [names2, colors2, outlines2, anim2] = buildNames(sel1);
        auto menu2 = std::make_shared<MenuText>();
        menu2->setStrings(names2, colors2, outlines2, anim2);
        menu2->setFontSize(28);
        menu2->arrange(0, 0, 0, 36);
        menu2->runAtPosition(100, 100);
        int sel2 = menu2->getResult();
        if (sel2 < 0 || sel2 >= (int)allies.size()) continue;    // cancelled second pick, retry
        if (sel2 == sel1) continue;    // same role, retry

        // Perform swap
        auto* a = allies[sel1];
        auto* b = allies[sel2];
        int ax = a->X(), ay = a->Y();
        int bx = b->X(), by = b->Y();
        a->setPositionOnly(bx, by);
        b->setPositionOnly(ax, ay);
        auto pa = pos45To90(bx, by);
        auto pb = pos45To90(ax, ay);
        a->Pos.x = pa.x;
        a->Pos.y = pa.y;
        b->Pos.x = pb.x;
        b->Pos.y = pb.y;
    }
    swapSelected_ = nullptr;
    positionSwapActive_ = false;
}

void BattleSceneHades::backRun1()
{
    // 更新受击闪红计时器
    for (auto it = hurt_flash_timers_.begin(); it != hurt_flash_timers_.end();)
    {
        it->second--;
        if (it->second <= 0)
        {
            it = hurt_flash_timers_.erase(it);
        }
        else
        {
            ++it;
        }
    }

    if (slow_ > 0)
    {
        if (current_frame_ % 4) { return; }
        //x_ = rand_.rand_int(2) - rand_.rand_int(2);
        //y_ = rand_.rand_int(2) - rand_.rand_int(2);
        slow_--;
    }
    ultHitRoles_.clear();
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

        // Combo: per-frame effects
        {
            auto& cs = KysChess::ChessCombo::getMutableStates();
            auto it = cs.find(r->ID);
            if (it != cs.end() && r->Dead == 0)
            {
                auto& s = it->second;

                // Freeze reduction (base + shield bonus)
                int totalFreezeRes = s.freezeReductionPct + (s.shield > 0 ? s.shieldFreezeResPct : 0);
                if (totalFreezeRes > 0 && r->Frozen > 0)
                    r->Frozen = std::max(0, (int)(r->Frozen * (1.0 - totalFreezeRes / 100.0)));

                // Poison DOT tick (every 30 frames)
                if (s.poisonTimer > 0)
                {
                    s.poisonTimer--;
                    if (current_frame_ % 30 == 0)
                    {
                        int dmg = std::max(1, r->HP * s.poisonTickDmg / 100);
                        r->HurtThisFrame += dmg;
                        TextEffect te;
                        te.set(std::to_string(-dmg), {0, 200, 0, 255}, r);
                        text_effects_.push_back(std::move(te));
                    }
                }

                if (s.bleedStacks > 0)
                {
                    if (s.bleedTimer > 0)
                        s.bleedTimer--;
                    if (s.bleedTimer <= 0)
                    {
                        int dmg = std::max(1, r->MaxHP * s.bleedStacks / 100);
                        r->HurtThisFrame += dmg;
                        TextEffect te;
                        te.set(std::to_string(-dmg), {180, 0, 0, 255}, r);
                        text_effects_.push_back(std::move(te));
                        if (!s.bleedPersistFlag)
                            s.bleedStacks = std::max(0, s.bleedStacks - 1);
                        if (s.bleedStacks > 0)
                            s.bleedTimer = 10;
                        else
                        {
                            s.bleedTimer = 0;
                            s.bleedPersistFlag = false;
                        }
                    }
                }
                else
                {
                    s.bleedTimer = 0;
                    s.bleedPersistFlag = false;
                }

                if (s.mpBlockTimer > 0)
                    s.mpBlockTimer--;

                // Periodic damage immunity
                if (s.damageImmunityAfterFrames > 0)
                {
                    if (s.damageImmunityTimer > 0)
                        s.damageImmunityTimer--;
                    if (s.damageImmunityTimer <= 0)
                    {
                        r->Invincible = std::max(r->Invincible, s.damageImmunityDuration);
                        s.damageImmunityTimer = s.damageImmunityAfterFrames;
                    }
                }

                // Periodic auto ultimate
                if (s.autoUltimateAfterFrames > 0 && r->UsingMagic == nullptr && r->Dead == 0)
                {
                    if (s.autoUltimateTimer > 0)
                        s.autoUltimateTimer--;
                    if (s.autoUltimateTimer <= 0)
                    {
                        Magic* magic = selectMagic(r, std::greater<double>{});
                        if (magic)
                        {
                            createSkillAttackEffect(r, magic, true);
                            addFloatingText(r, std::string(magic->Name), {255, 215, 0, 255}, EMPHASIS_TEXT_SIZE);
                        }
                        s.autoUltimateTimer = s.autoUltimateAfterFrames;
                    }
                }

                // HP regen
                if (s.hpRegenPct > 0 && s.hpRegenInterval > 0 && current_frame_ % s.hpRegenInterval == 0)
                {
                    int heal = r->MaxHP * s.hpRegenPct / 100;
                    r->HP = std::min(r->MaxHP, r->HP + heal);
                }

                // Heal aura (heal nearby allies)
                if ((s.healAuraPct > 0 || s.healAuraFlat > 0) && s.healAuraInterval > 0 && current_frame_ % s.healAuraInterval == 0)
                {
                    for (auto ally : battle_roles_)
                    {
                        if (ally->Team == r->Team && ally->Dead == 0 && ally != r
                            && EuclidDis(ally->Pos, r->Pos) <= TILE_W * 6)
                        {
                            int heal = ally->MaxHP * s.healAuraPct / 100 + s.healAuraFlat;
                            ally->HP = std::min(ally->MaxHP, ally->HP + heal);
                            // HealedATKSPDBoost: reduce cooldown for healed allies
                            if (s.healedATKSPDBoostPct > 0 && ally->CoolDown > 0)
                                ally->CoolDown = static_cast<int>(ally->CoolDown * (1.0 - s.healedATKSPDBoostPct / 100.0));
                        }
                    }
                }

                // Compute lastAliveFlag
                {
                    s.lastAliveFlag = true;
                    for (auto ally : battle_roles_)
                        if (ally->Team == r->Team && ally != r && ally->Dead == 0) { s.lastAliveFlag = false; break; }
                }

                // Triggered effects: check conditions each frame
                for (int tei = 0; tei < (int)s.triggeredEffects.size(); ++tei)
                {
                    auto& te = s.triggeredEffects[tei];
                    if (te.maxCount > 0 && s.effectActivationCounts[tei] >= te.maxCount) continue;

                    bool active = false;
                    if (te.trigger == KysChess::Trigger::WhileLowHP)
                        active = r->HP * 100 / std::max(1, r->MaxHP) < te.triggerValue;
                    else if (te.trigger == KysChess::Trigger::LastAlive)
                        active = s.lastAliveFlag;
                    else if (te.trigger == KysChess::Trigger::AllyLowHPBurst)
                    {
                        if (r->HP * 100 / std::max(1, r->MaxHP) < te.triggerValue
                            && s.triggerTimers[te.trigger] <= 0)
                        {
                            s.effectActivationCounts[tei]++;
                            for (auto ally : battle_roles_)
                            {
                                if (ally->Team == r->Team && ally != r && ally->Dead == 0)
                                {
                                    auto ait2 = cs.find(ally->ID);
                                    if (ait2 != cs.end())
                                        ait2->second.triggerTimers[te.trigger] = te.duration;
                                }
                            }
                        }
                        continue;
                    }

                    if (active && te.type == KysChess::EffectType::HealBurst)
                    {
                        s.effectActivationCounts[tei]++;
                        int hpBefore = r->HP;
                        r->HP = std::min(r->MaxHP, r->HP + r->MaxHP * te.value / 100);
                        if (r->HP > hpBefore)
                            addRoleEffect(r, KysChess::EFT_HEAL, ROLE_STATUS_EFT_FRAMES);
                    }
                }

                // Ramping damage idle decay
                for (size_t i = 0; i < s.rampings.size(); i++)
                {
                    if (s.rampingIdleTimers[i] > 0) s.rampingIdleTimers[i]--;
                    else s.rampingStacks[i] = 0;
                }

                // Trigger timer countdowns
                for (auto& [trig, timer] : s.triggerTimers)
                    if (timer > 0) timer--;
            }
        }

        if (r->Frozen > 0)
        {
            continue;
        }

        {
            auto& cs = KysChess::ChessCombo::getMutableStates();
            auto it = cs.find(r->ID);
            if (it != cs.end() && it->second.postSkillDashTimer > 0)
            {
                Role* enemy = findNearestEnemy(r->Team, r->Pos);
                if (enemy)
                {
                    auto away = r->Pos - enemy->Pos;
                    if (away.norm() < 0.01)
                        away = Pointf(
                            static_cast<float>(rand_.rand() - 0.5),
                            static_cast<float>(rand_.rand() - 0.5));

                    if (away.norm() > 0.01)
                    {
                        double speed = std::max(4.0, r->Speed / 10.0);
                        double angle = away.getAngle();
                        const double offsets[] = {0.0, M_PI / 6.0, -M_PI / 6.0, M_PI / 3.0, -M_PI / 3.0};
                        bool foundPath = false;
                        for (double offset : offsets)
                        {
                            Pointf dash(
                                static_cast<float>(std::cos(angle + offset)),
                                static_cast<float>(std::sin(angle + offset)));
                            dash.normTo(speed);
                            if (!canWalk90(r->Pos + dash, r)) continue;

                            r->Velocity = dash;
                            r->RealTowards = dash;
                            r->RealTowards.normTo(1);
                            foundPath = true;
                            break;
                        }
                        if (!foundPath)
                            r->Velocity = {0, 0, 0};
                    }
                }
                it->second.postSkillDashTimer = std::max(0, it->second.postSkillDashTimer - 1);
            }
        }

        //更新速度，加速度，力学位置
        {
            auto p = r->Pos + r->Velocity;
            int dis = -1;
            if (r->OperationType == 3) { dis = 1; }

            auto& path_info = paths_[r];
            if (canWalk90(p, r, dis))
            {
                r->Pos = p;
                path_info.frames_following++;
            }
            else
            {
                // Try sliding along obstacles
                bool can_slide = false;

                // Try X-only movement
                auto px = r->Pos;
                px.x = p.x;
                if (canWalk90(px, r, dis)) {
                    r->Pos = px;
                    r->Velocity.y = 0;
                    can_slide = true;
                    path_info.frames_sliding++;
                }
                // Try Y-only movement
                else {
                    auto py = r->Pos;
                    py.y = p.y;
                    if (canWalk90(py, r, dis)) {
                        r->Pos = py;
                        r->Velocity.x = 0;
                        can_slide = true;
                        path_info.frames_sliding++;
                    }
                }

                if (!can_slide) {
                    r->Velocity = { 0, 0, 0 };
                    path_info.frames_stuck++;
                }
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
        {
            int prevCD = r->CoolDown;
            if (r->Frozen == 0)
            {
                decreaseToZero(r->CoolDown);
            }
            // Combo: post-skill invincibility (逍遥至尊)
            if (prevCD > 0 && r->CoolDown == 0)
            {
                auto& cs = KysChess::ChessCombo::getMutableStates();
                auto it = cs.find(r->ID);
                if (it != cs.end() && r->Dead == 0)
                {
                    if (it->second.postSkillInvincFrames > 0)
                        r->Invincible = std::max(r->Invincible, it->second.postSkillInvincFrames);
                    if (it->second.postSkillDash && it->second.postSkillDashPending)
                    {
                        it->second.postSkillDashTimer = std::max(1,
                            it->second.postSkillDashFrames > 0 ? it->second.postSkillDashFrames : POST_SKILL_DASH_FRAMES);
                        it->second.postSkillDashPending = false;
                    }
                    if (it->second.onSkillTeamHealPending && it->second.onSkillTeamHeal > 0)
                    {
                        for (auto ally : battle_roles_)
                        {
                            if (ally->Team == r->Team && ally->Dead == 0)
                            {
                                int hpBefore = ally->HP;
                                ally->HP = std::min(ally->MaxHP, ally->HP + it->second.onSkillTeamHeal);
                                if (ally->HP > hpBefore)
                                    addRoleEffect(ally, KysChess::EFT_HEAL, ROLE_STATUS_EFT_FRAMES);
                            }
                        }
                    }
                    it->second.onSkillTeamHealPending = false;
                }
            }
        }
        if (r->CoolDown == 0)
        {
            if (current_frame_ % 3 == 0)
            {
                r->PhysicalPower += 1;
            }
            r->ActFrame = 0;
            //r->OperationType = -1;
            r->ActType = -1;
            r->HaveAction = 0;
        }
        if (current_frame_ % 3 == 0) { 
            changeRoleMP(r, 1);
        }
        decreaseToZero(r->HurtFrame);
        decreaseToZero(r->Attention);
        decreaseToZero(r->Invincible);
    }

    //if (current_frame_ % 2 == 0)
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

    //效果
    //if (current_frame_ % 2 == 0)
    {
        for (auto& ae : attack_effects_)
        {
            ae.Frame++;
            ae.Velocity += ae.Acceleration;
            ae.Pos += ae.Velocity;
            bool scriptedImpact = hasScriptedImpact(ae);
            Role* r = nullptr;
            if (ae.Attacker)
            {
                if (ae.PreferredTarget && ae.PreferredTarget->Dead == 0 && ae.PreferredTarget->Team != ae.Attacker->Team)
                    r = ae.PreferredTarget;
                else if (!ae.RequirePreferredTarget)
                    r = findNearestEnemy(ae.Attacker->Team, ae.Pos);
                else
                {
                    ae.NoHurt = 1;
                    ae.Frame = std::max(ae.TotalFrame - 5, ae.Frame);
                }
            }
            if (ae.Track && r)
            {
                //追踪
                double n = ae.Velocity.norm();
                auto p = (r->Pos - ae.Pos).normTo(n / 20.0);
                ae.Velocity += p;
                ae.Velocity.normTo(n);
            }
            //是否打中了敌人
            if (r && !r->HurtFrame
                && (!r->Invincible || attackHasExecuteEffect(ae))
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
                if (!scriptedImpact)
                {
                    // Combo: dodge check
                    {
                        auto& cs = KysChess::ChessCombo::getMutableStates();
                        auto dit = cs.find(r->ID);
                        if (dit != cs.end() && dit->second.dodgeChancePct > 0
                            && rand_.rand() * 100 < dit->second.dodgeChancePct)
                        {
                            dit->second.dodgedLast = true;
                            addRoleEffect(r, KysChess::EFT_EVADE, ROLE_STATUS_EFT_FRAMES);
                            ae.Defender[r]++;
                            continue;
                        }
                    }
                }

                ae.Defender[r]++;
                if (scriptedImpact)
                {
                    r->Shake = 5;
                    applyScriptedAttackEffect(ae, r);
                }
                else
                {
                    shake_ = ae.IsUltimate ? 10 : 0;
                    r->Frozen = 0;
                    if (ae.IsMain) applyFrozen(r, ae.IsUltimate ? 10 : 5);
                    r->Shake = ae.IsUltimate ? 10 : 5;
                    if (ae.IsUltimate) { ultHitRoles_.insert(r); }
                    if (ae.OperationType >= 0)
                    {
                        Engine::getInstance()->gameControllerRumble(100, 100, 50);
                        defaultMagicEffect(ae, r);
                    }
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
                if (!ae1.UsingMagic || ae1.IgnoreProjectileCancel)
                {
                    continue;
                }
                for (int j = i + 1; j < attack_effects_.size() - 1; j++)
                {
                    auto& ae2 = attack_effects_[j];
                    if (!ae2.UsingMagic || ae2.IgnoreProjectileCancel)
                    {
                        continue;
                    }
                    constexpr int PROJECTILE_GRACE_FRAMES = 5;
                    
                    if (ae1.NoHurt == 0 && ae2.NoHurt == 0 && ae1.Attacker && ae2.Attacker
                        && ae1.Frame >= PROJECTILE_GRACE_FRAMES && ae2.Frame >= PROJECTILE_GRACE_FRAMES
                        && ae1.Attacker->Team != ae2.Attacker->Team && EuclidDis(ae1.Pos, ae2.Pos) < TILE_W * 2
                        && !ae1.IsUltimate && !ae2.IsUltimate)
                    {
                        //LOG("{} beat {}, ", ae1.UsingMagic->Name, ae2.UsingMagic->Name);
                        int hurt1 = calMagicHurt(ae1.Attacker, ae2.Attacker, ae1.UsingMagic);
                        int hurt2 = calMagicHurt(ae2.Attacker, ae1.Attacker, ae2.UsingMagic);
                        ae1.Weaken += hurt2;
                        ae2.Weaken += hurt1;
                        ae1.Attacker->CancelDmg += hurt1;
                        ae2.Attacker->CancelDmg += hurt2;
                        if (ae1.Weaken > hurt1)
                        {
                            //直接设置帧数，后面就会删掉了
                            ae1.NoHurt = 1;
                            ae1.Frame = std::max(ae1.TotalFrame - 5, ae1.Frame);
                        }
                        if (ae2.Weaken > hurt2)
                        {
                            ae2.NoHurt = 1;
                            ae2.Frame = std::max(ae2.TotalFrame - 5, ae2.Frame);
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
    }

    //此处计算累积伤害
    auto isOccupied45 = [&](int x, int y, const Role* ignore) {
        for (auto role : battle_roles_)
        {
            if (role == ignore || role->Dead != 0) continue;
            auto pos = pos90To45(role->Pos.x, role->Pos.y);
            if (pos.x == x && pos.y == y)
                return true;
        }
        return false;
    };
    auto isUnattended = [&](Role* target, int team) {
        for (auto role : battle_roles_)
        {
            if (role->Dead != 0 || role->Team != team) continue;
            if (EuclidDis(role->Pos, target->Pos) <= TILE_W * 3)
                return false;
        }
        return true;
    };
    auto spawnBasicAttackProjectile = [&](Role* attacker, Role* target) {
        if (!attacker || !target || attacker->Dead != 0 || target->Dead != 0)
            return;

        Magic* basicMagic = Save::getInstance()->getMagic(1);
        if (!basicMagic)
            return;

        auto direction = target->Pos - attacker->Pos;
        if (direction.norm() <= 0.01)
            return;

        direction.normTo(1);
        attacker->RealTowards = direction;
        setFaceTowardsNearest(attacker);

        AttackEffect projectile;
        projectile.Attacker = attacker;
        projectile.UsingMagic = basicMagic;
        projectile.PreferredTarget = target;
        projectile.RequirePreferredTarget = 1;
        projectile.Track = 1;
        projectile.IsMain = 0;
        projectile.OperationType = 0;
        projectile.setEft(11);
        projectile.Pos = attacker->Pos + TILE_W * 2.0 * direction;
        projectile.Velocity = target->Pos - projectile.Pos;
        if (projectile.Velocity.norm() <= 0.01)
            projectile.Velocity = direction;
        projectile.Velocity.normTo(PROJECTILE_SPEED);
        projectile.TotalFrame = std::max(20,
            static_cast<int>(std::ceil(EuclidDis(target->Pos, projectile.Pos) / static_cast<double>(PROJECTILE_SPEED))) + 15);
        projectile.Frame = 0;
        attack_effects_.push_back(std::move(projectile));
        Audio::getInstance()->playESound(attacker->ActType);
    };
    auto tryForcePull = [&](Role* pulled, int pullerTeam, Role* reference, bool executeMode) {
        if (!pulled || !reference || pulled->Dead != 0) return false;

        auto& cs = KysChess::ChessCombo::getMutableStates();
        std::vector<Role*> candidates;
        for (auto role : battle_roles_)
        {
            if (role->Dead != 0 || role->Team != pullerTeam) continue;
            auto it = cs.find(role->ID);
            if (it == cs.end() || it->second.isSummonedClone) continue;
            if (executeMode)
            {
                if (!it->second.forcePullExecute || it->second.forcePullExecuteUsed) continue;
            }
            else
            {
                if (!it->second.forcePullProtect || it->second.forcePullProtectUsed) continue;
            }
            candidates.push_back(role);
        }

        std::sort(candidates.begin(), candidates.end(), [&](Role* lhs, Role* rhs) {
            return EuclidDis(lhs->Pos, reference->Pos) < EuclidDis(rhs->Pos, reference->Pos);
        });

        auto currentPos45 = pos90To45(pulled->Pos.x, pulled->Pos.y);
        for (auto puller : candidates)
        {
            auto center = pos90To45(puller->Pos.x, puller->Pos.y);
            std::vector<Point> cells;
            for (int dx = -1; dx <= 1; ++dx)
            {
                for (int dy = -1; dy <= 1; ++dy)
                {
                    if (dx == 0 && dy == 0) continue;
                    int x = center.x + dx;
                    int y = center.y + dy;
                    if (!canWalk45(x, y) || isOccupied45(x, y, pulled)) continue;
                    if (x == currentPos45.x && y == currentPos45.y) continue;
                    cells.push_back({x, y});
                }
            }
            if (cells.empty()) continue;

            std::sort(cells.begin(), cells.end(), [&](const Point& lhs, const Point& rhs) {
                return std::abs(lhs.x - currentPos45.x) + std::abs(lhs.y - currentPos45.y)
                    < std::abs(rhs.x - currentPos45.x) + std::abs(rhs.y - currentPos45.y);
            });

            const auto& cell = cells.front();
            int x = cell.x;
            int y = cell.y;
            auto pos = pos45To90(x, y);
            pulled->setPositionOnly(x, y);
            pulled->Pos.x = pos.x;
            pulled->Pos.y = pos.y;
            pulled->Pos.z = 0;
            pulled->Velocity = {0, 0, 0};
            pulled->Acceleration = {0, 0, gravity_};
            pulled->FindingWay = 0;
            setFaceTowardsNearest(pulled);
            setFaceTowardsNearest(puller);

            auto& pullState = cs[puller->ID];
            if (executeMode)
            {
                pullState.forcePullExecuteUsed = true;
                spawnBasicAttackProjectile(puller, pulled);
            }
            else
            {
                pullState.forcePullProtectUsed = true;
                int hpBefore = pulled->HP;
                int heal = std::max(1, pulled->MaxHP * 10 / 100);
                pulled->HP = std::min(pulled->MaxHP, pulled->HP + heal);
                pulled->Invincible += 10;
                if (pulled->HP > hpBefore)
                    addRoleEffect(pulled, KysChess::EFT_HEAL, ROLE_STATUS_EFT_FRAMES);
            }

            addFloatingText(pulled, "挪移", {160, 220, 255, 255}, STATUS_TEXT_SIZE);
            return true;
        }
        return false;
    };
    for (auto r : battle_roles_)
    {
        int hurt = r->HurtThisFrame;
        if (hurt > 0)
        {
            r->HurtThisFrame = 0;

            int hpBefore = r->HP;
            bool isUlt = ultHitRoles_.count(r) > 0;
            bool executedHit = execution_popup_roles_.erase(r->ID) > 0;
            if (executedHit)
            {
                addFloatingText(r, "9999", {255, 136, 48, 255}, ULT_DAMAGE_TEXT_SIZE);
            }
            else
            {
                TextEffect te;
                Color c = isUlt ? Color{255, 215, 0, 255} : (r->Team == 0 ? Color{255, 20, 20, 255} : Color{255, 255, 255, 255});
                te.set(std::to_string(-hurt), c, r);
                if (isUlt) te.Size = ULT_DAMAGE_TEXT_SIZE;
                text_effects_.push_back(std::move(te));
            }
                AttackEffect ae1;
                ae1.FollowRole = r;
                //ae1.EffectNumber = eft[rand_.rand() * eft.size()];
                ae1.setPath(std::format("eft/bld{:03}", int(rand_.rand() * 5)));
                ae1.TotalFrame = ae1.TotalEffectFrame;
                ae1.Frame = 0;
                attack_effects_.push_back(std::move(ae1));
                r->HP -= hurt;
                hurt_flash_timers_[r->ID] = HURT_FLASH_DURATION;
                double mpGain = (hurt / r->MaxHP)  * 75.0;
                changeRoleMP(r, mpGain);
                if (r->HP <= 0 && r->Dead == 0)
                {
                    auto& cs = KysChess::ChessCombo::getMutableStates();
                    auto sit = cs.find(r->ID);
                    if (sit != cs.end() && sit->second.deathPrevention && !sit->second.deathPreventionUsed)
                    {
                        sit->second.deathPreventionUsed = true;
                        r->HP = 1;
                        r->Invincible = std::max(r->Invincible,
                            sit->second.deathPreventionFrames > 0 ? sit->second.deathPreventionFrames : 100);
                    }
                    else
                    {
                        //LOG("{} has been beat\n", r->Name);
                        r->Dead = 1;
                        r->HP = 0;
                        if (sit != cs.end())
                            sit->second.onSkillTeamHealPending = false;
                        tracker_.recordKill(r->LastAttacker, r, current_frame_);
                        tracker_.recordDeath(r, current_frame_);

                        // Combo: kill-heal and kill-invincibility
                        if (r->LastAttacker)
                        {
                            auto kit = cs.find(r->LastAttacker->ID);
                            if (kit != cs.end())
                            {
                                if (kit->second.killHealPct > 0)
                                    r->LastAttacker->HP = std::min(r->LastAttacker->MaxHP,
                                        r->LastAttacker->HP + r->LastAttacker->MaxHP * kit->second.killHealPct / 100);
                                if (kit->second.killInvincFrames > 0)
                                    r->LastAttacker->Invincible = kit->second.killInvincFrames;
                                // Bloodlust: permanent ATK per kill
                                if (kit->second.bloodlustATKPerKill > 0)
                                    r->LastAttacker->Attack += kit->second.bloodlustATKPerKill;
                            }
                        }

                        if (sit != cs.end() && sit->second.deathAOEPct > 0)
                        {
                            int aoeDmg = std::max(1, r->MaxHP * sit->second.deathAOEPct / 100);
                            spawnAreaImpactProjectiles(r, r, 9, 9, KysChess::EFT_DEATH_BLAST, aoeDmg, sit->second.deathAOEStunFrames);
                        }

                        for (auto ally : battle_roles_)
                        {
                            if (ally == r || ally->Team != r->Team || ally->Dead != 0)
                                continue;

                            auto ait = cs.find(ally->ID);
                            if (ait == cs.end())
                                continue;

                            auto& as = ait->second;
                            if (as.allyDeathStatBoost > 0
                                && rolesShareCombo(ally, r, {"明教", "四大法王"}))
                            {
                                ally->Attack += as.allyDeathStatBoost;
                                ally->Defence += as.allyDeathStatBoost;
                                // ally->Speed += as.allyDeathStatBoost;
                            }

                            if (as.shieldOnAllyDeathCount > 0
                                && rolesShareCombo(ally, r, {"全真教"}))
                            {
                                as.shieldOnAllyDeathTracker++;
                                if (as.shieldOnAllyDeathTracker >= as.shieldOnAllyDeathCount)
                                {
                                    as.shieldOnAllyDeathTracker = 0;
                                    if (as.shieldPctMaxHP > 0)
                                        as.shield = std::max(as.shield, ally->MaxHP * as.shieldPctMaxHP / 100);
                                }
                            }
                        }

                        // Transfer 独行 to next strongest alive member
                        KysChess::ChessCombo::transferAntiCombo(r->ID, battle_roles_);

                        //r->Velocity = r->Pos - ae1.Attacker->Pos;
                        r->Velocity.normTo(15);    //因为已经有击退速度，可以直接利用
                        r->Velocity.z = 12;
                        //r->Velocity.normTo(std::min(hurt / 1.0, 15.0));
                        r->Velocity.normTo(hurt / 2.0);
                        r->Frozen = 0;
                        applyFrozen(r, 5);
                        x_ = rand_.rand_int(2) - rand_.rand_int(2);
                        y_ = rand_.rand_int(2) - rand_.rand_int(2);
                        //dying_ = r;
                        pos_ = r->Pos;
                        frozen_ = 5;
                        shake_ = 10;
                        slow_ = 10;
                        close_up_ = 30;
                    }
                }

                if (r->Dead == 0)
                {
                    if (r->MaxHP > 0
                        && hpBefore * 4 > r->MaxHP
                        && r->HP * 4 <= r->MaxHP
                        && r->LastAttacker
                        && r->LastAttacker->Dead == 0
                        && r->LastAttacker->Team != r->Team)
                    {
                        tryForcePull(r, r->Team, r, false);
                    }

                    if (r->MaxHP > 0
                        && hpBefore * 100 > r->MaxHP * 15
                        && r->HP * 100 <= r->MaxHP * 15
                        && isUnattended(r, 1 - r->Team))
                    {
                        tryForcePull(r, 1 - r->Team, r, true);
                    }
                }
            }
            r->HP = GameUtil::limit(r->HP, 0, r->MaxHP);
            r->MP = GameUtil::limit(r->MP, 0, GameUtil::MAX_MP);
            r->PhysicalPower = GameUtil::limit(r->PhysicalPower, 0, 100);
        }

    //处理文字
    {
        for (auto& te : text_effects_)
        {
            if (te.Type == 0) { te.Pos.y -= 2; }
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
        while (!enemies_.empty())
        {
            battle_roles_.push_back(enemies_.front());
            enemies_.pop_front();
            // battle_roles_.back()->Attention = 30;
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

        if (battle_result >= 0)
        {
            if (result_ == -1)
            {
                //pos_ = dying_->Pos;
                close_up_ = 60;
                frozen_ = 60;
                slow_ = 40;
                shake_ = 60;
                result_ = battle_result;
                tracker_.recordBattleEnd(current_frame_, battle_result);
            }
            if (slow_ == 0 && (result_ == 0 || result_ == 1))
            {
                auto* engine = Engine::getInstance();
                if (!battle_log_shown_)
                {
                    engine->showBattleLogWindow(buildBattleLogData());
                    battle_log_shown_ = true;
                }
                if (!engine->isBattleLogWindowOpen())
                {
                    calExpGot();
                    setExit(true);
                }
            }
        }
    }
}

void BattleSceneHades::Action(Role* r)
{
    if (r->HaveAction)
    {
        //音效和动画
        if (r->OperationType >= 0
            //&& r->ActFrame == r->FightFrame[r->ActType] - 3
            && r->ActFrame == calCast(r->ActType, r->OperationType, r))
        {
            //r->HaveAction = 0;
            r->PreActTimer = current_frame_;
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

            {
                auto& cs = KysChess::ChessCombo::getMutableStates();
                auto sit = cs.find(r->ID);
                if (sit != cs.end() && sit->second.blinkAttack)
                {
                    // auto target = findNearestEnemy(r->Team, r->Pos);
                    auto& blinkState = sit->second;
                    auto target = blinkState.blinkAttackUseWeakest
                        ? findWeakestVulnerableEnemy(r->Team)
                        : findRandomEnemy(r->Team);
                    if (!target)
                        target = findRandomEnemy(r->Team);
                    if (target)
                    {
                        blinkState.blinkAttackUseWeakest = !blinkState.blinkAttackUseWeakest;
                        auto targetPos45 = pos90To45(target->Pos.x, target->Pos.y);
                        auto currentPos45 = pos90To45(r->Pos.x, r->Pos.y);
                        int gridReach = std::max(1, static_cast<int>(calcBlinkReach(magic) / TILE_W) + 1);
                        double blinkReach = calcBlinkReach(magic);
                        std::vector<Point> cells;
                        for (int dx = -gridReach; dx <= gridReach; ++dx)
                        {
                            for (int dy = -gridReach; dy <= gridReach; ++dy)
                            {
                                int x = targetPos45.x + dx;
                                int y = targetPos45.y + dy;
                                if (x == currentPos45.x && y == currentPos45.y) continue;
                                if (!canWalk45(x, y)) continue;

                                auto pos = pos45To90(x, y);
                                if (EuclidDis(pos, target->Pos) > blinkReach) continue;

                                bool occupied = false;
                                for (auto role : battle_roles_)
                                {
                                    if (role == r || role->Dead != 0) continue;
                                    auto rolePos45 = pos90To45(role->Pos.x, role->Pos.y);
                                    if (rolePos45.x == x && rolePos45.y == y)
                                    {
                                        occupied = true;
                                        break;
                                    }
                                }
                                if (!occupied)
                                    cells.push_back({x, y});
                            }
                        }

                        if (!cells.empty())
                        {
                            const auto& cell = cells[rand_.rand_int(static_cast<int>(cells.size()))];
                            int x = cell.x;
                            int y = cell.y;
                            auto pos = pos45To90(x, y);
                            r->setPositionOnly(x, y);
                            r->Pos.x = pos.x;
                            r->Pos.y = pos.y;
                            r->Pos.z = 0;
                            r->Velocity = {0, 0, 0};
                            r->Acceleration = {0, 0, gravity_};
                            r->FindingWay = 0;
                            Audio::getInstance()->playESound(BLINK_SOUND_EFFECT_ID);
                            setFaceTowardsNearest(r);
                            r->RealTowards = target->Pos - r->Pos;
                            if (r->RealTowards.norm() > 0.01)
                                r->RealTowards.normTo(1);
                        }
                    }
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
            // r->PhysicalPower = GameUtil::limit(r->PhysicalPower - 3, 0, Role::getMaxValue()->PhysicalPower);
            ae.TotalFrame = 30;
            //r->CoolDown += ae.TotalFrame;
            ae.Attacker = r;
            ae.IsUltimate = ultCasters_.count(r) ? 1 : 0;
            int extraUltimateProjectiles = ae.IsUltimate ? getUltimateExtraProjectileCount(r) : 0;
            if (ae.IsUltimate)
            {
                auto& cs = KysChess::ChessCombo::getMutableStates();
                auto it = cs.find(r->ID);
                if (it != cs.end())
                {
                    if (it->second.onSkillTeamHeal > 0)
                        it->second.onSkillTeamHealPending = true;
                    if (it->second.postSkillDash)
                        it->second.postSkillDashPending = true;
                }
            }
            r->RealTowards.normTo(1);
            ae.Pos = r->Pos + TILE_W * 2.0 * r->RealTowards;
            ae.Frame = 0;
            ae.OperationType = r->OperationType;
            if (r->OperationType == -1)
            {
                ae.OperationType = getOperationType(ae.UsingMagic->AttackAreaType);
            }

            // Chess mod: no in-battle magic level-up
            //根据性质创造攻击效果
            //连击计数：超时或已触发终结技则重置
            if (current_frame_ - r->PreActTimer > 120 || r->OperationCount >= 3)
            {
                r->OperationCount = 0;
            }
            if (ae.OperationType == 0)
            {
                r->OperationCount++;
                ae.TotalFrame = 10;
                if (ae.IsUltimate || r->OperationCount >= 3)
                {
                    ae.TotalFrame = 30;
                    ae.Strengthen = 2;
                    ae.Velocity = r->RealTowards;
                    ae.Velocity.normTo(magic->SelectDistance / 2.0);
                    ae.Track = 1;
                    r->OperationCount = 0;
                }
                attack_effects_.push_back(ae);
                // Ulti splash: spawn 1 extra homing projectiles
                if (ae.IsUltimate)
                {
                    auto splash = ae;
                    splash.Strengthen = 0.5;
                    splash.Track = 1;
                    splash.TotalFrame = 60;
                    splash.Velocity = r->RealTowards;
                    splash.Velocity.normTo(3);
                    spawnTrackingProjectileSpread(splash, 1, 5, 5);
                }
                spawnUltimateExtraProjectiles(ae, extraUltimateProjectiles);
            }
            else if (ae.OperationType == 1)
            {
                int count = 1;
                if (ae.IsUltimate) count *= 2;
                auto p = ae.Pos;
                auto projectilePrototype = ae;
                projectilePrototype.TotalFrame = 120;
                projectilePrototype.Track = 1;
                projectilePrototype.Pos = p;
                projectilePrototype.Velocity = r->RealTowards;
                projectilePrototype.Velocity.normTo(3);
                projectilePrototype.Frame = 0;
                spawnTrackingProjectileSpread(projectilePrototype, count, 0, 0, 10);
                spawnUltimateExtraProjectiles(projectilePrototype, extraUltimateProjectiles);
            }
            else if (ae.OperationType == 2)
            {
                auto r0 = findNearestEnemy(r->Team, r->Pos);
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
                ae.Velocity.normTo(PROJECTILE_SPEED);
                ae.TotalFrame = calcProjectileFrames(magic->SelectDistance, TILE_W * 2);
                if (magic->AttackAreaType == 1 || magic->AttackAreaType == 2)
                {
                    ae.Through = 1;
                }
                attack_effects_.push_back(ae);
                spawnUltimateExtraProjectiles(ae, extraUltimateProjectiles);
                if (magic->AttackAreaType == 1 || magic->AttackAreaType == 2)
                {
                    int sideCount = ae.IsUltimate ? 3 : 2;
                    double sideStr = ae.IsUltimate ? 0.35 : 0.2;
                    double v = 5;
                    double angle = ae.Velocity.getAngle();
                    for (int i = 0; i < sideCount; i++)
                    {
                        v -= 0.5 / sideCount * 2;
                        float a = angle - 15.0 / 180 * M_PI + rand_.rand() * 30.0 / 180 * M_PI;
                        ae.Velocity = { cos(a), sin(a) };
                        ae.Velocity.normTo(v);
                        ae.Through = 1;
                        ae.Strengthen = sideStr;
                        ae.IsMain = 0;
                        attack_effects_.push_back(ae);
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
                int count = 1;
                double multiHitScore = (r->Speed + r->getActProperty(ae.UsingMagic->MagicType)) / 180.0;
                if (rand_.rand() < multiHitScore) count++;
                if (rand_.rand() < multiHitScore * 0.5) count++;
                for (int i = 0; i < count; i++)
                {
                    ae.Pos = p + r->Velocity * (i - 1) * 2;
                    ae.Frame += 3;
                    attack_effects_.push_back(ae);
                }
                auto projectilePrototype = ae;
                projectilePrototype.Pos = p;
                projectilePrototype.Frame = 0;
                projectilePrototype.Track = 1;
                spawnUltimateExtraProjectiles(projectilePrototype, extraUltimateProjectiles);
            }
            // LOG("{} team {} use {} as {}\n", ae.Attacker->Name, ae.Attacker->Team, ae.UsingMagic->Name, ae.OperationType);
            changeRoleMP(r, ultCasters_.count(r) ? -r->MaxMP : 5);
            ultCasters_.erase(r);
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
                auto r0 = findFarthestEnemy(r->Team, r->Pos);
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

        if (r->OperationType == 1)
        {
            r->ActFrame++;
        }
        else
        {
            r->ActFrame++;
        }
    }
}

void BattleSceneHades::createSkillAttackEffect(Role* r, Magic* magic, bool isUltimate)
{
    AttackEffect ae;
    Audio::getInstance()->playASound(magic->SoundID);
    ae.setEft(magic->EffectID);
    ae.UsingMagic = magic;
    ae.TotalFrame = 30;
    ae.Attacker = r;
    ae.IsUltimate = isUltimate ? 1 : 0;

    if (isUltimate)
    {
        ultCasters_.insert(r);
        auto& cs = KysChess::ChessCombo::getMutableStates();
        auto it = cs.find(r->ID);
        if (it != cs.end())
        {
            if (it->second.onSkillTeamHeal > 0)
                it->second.onSkillTeamHealPending = true;
            if (it->second.postSkillDash)
                it->second.postSkillDashPending = true;
        }
    }

    r->RealTowards.normTo(1);
    ae.Pos = r->Pos + TILE_W * 2.0 * r->RealTowards;
    ae.Frame = 0;
    ae.OperationType = getOperationType(magic->AttackAreaType);
    attack_effects_.push_back(ae);
    if (isUltimate)
        spawnUltimateExtraProjectiles(ae, getUltimateExtraProjectileCount(r));
}

int BattleSceneHades::getUltimateExtraProjectileCount(Role* r) const
{
    if (!r) return 0;
    auto& cs = KysChess::ChessCombo::getActiveStates();
    auto it = cs.find(r->ID);
    if (it == cs.end())
        return 0;
    return std::max(0, it->second.ultimateExtraProjectiles);
}

void BattleSceneHades::spawnUltimateExtraProjectiles(const AttackEffect& prototype, int extraCount)
{
    if (extraCount <= 0 || !prototype.IsUltimate || !prototype.Attacker)
        return;

    auto launch = prototype;
    launch.Defender.clear();
    launch.NoHurt = 0;
    launch.Weaken = 0;
    launch.IsMain = 0;
    spawnTrackingProjectileSpread(launch, extraCount);
}

void BattleSceneHades::spawnAreaImpactProjectiles(Role* attacker,
                                                  Role* origin,
                                                  int width,
                                                  int height,
                                                  int eftId,
                                                  int damage,
                                                  int stunFrames)
{
    if (!origin || damage <= 0)
        return;

    Role* source = attacker ? attacker : origin;
    for (auto enemy : battle_roles_)
    {
        if (!enemy || enemy == origin || enemy->Dead != 0 || enemy->Team == origin->Team)
            continue;
        if (!isWithinGridArea(origin, enemy, width, height))
            continue;

        auto direction = enemy->Pos - origin->Pos;
        if (direction.norm() <= 0.01)
            direction = {1, 0, 0};
        direction.normTo(1);

        AttackEffect blast;
        blast.Attacker = source;
        blast.PreferredTarget = enemy;
        blast.ScriptedDamage = damage;
        blast.ScriptedStunFrames = stunFrames;
        blast.Track = 1;
        blast.Through = 0;
        blast.IsMain = 0;
        blast.IgnoreProjectileCancel = 1;
        blast.RequirePreferredTarget = 1;
        blast.OperationType = 2;
        blast.setEft(eftId);
        auto spawnOffset = direction;
        spawnOffset.normTo(TILE_W * 1.5);
        blast.Pos = origin->Pos + spawnOffset;
        blast.Velocity = enemy->Pos - blast.Pos;
        if (blast.Velocity.norm() <= 0.01)
            blast.Velocity = direction;
        blast.Velocity.normTo(PROJECTILE_SPEED);
        blast.TotalFrame = std::max(15,
            static_cast<int>(std::ceil(EuclidDis(enemy->Pos, blast.Pos) / static_cast<double>(PROJECTILE_SPEED))) + 15);
        blast.Frame = 0;
        attack_effects_.push_back(std::move(blast));
    }
}

void BattleSceneHades::spawnTrackingProjectileSpread(const AttackEffect& prototype,
                                                     int projectileCount,
                                                     int initialFrame,
                                                     int frameStep,
                                                     int randomFrameRange)
{
    if (projectileCount <= 0 || !prototype.Attacker)
        return;

    auto launch = prototype;
    double speed = launch.Velocity.norm();
    if (speed <= 0.01)
        speed = (launch.OperationType == 2) ? PROJECTILE_SPEED : 3.0;

    auto direction = launch.Velocity;
    if (direction.norm() <= 0.01)
    {
        direction = launch.Attacker->RealTowards;
        if (direction.norm() <= 0.01)
        {
            if (auto target = findNearestEnemy(launch.Attacker->Team, launch.Pos))
                direction = target->Pos - launch.Pos;
        }
    }
    if (direction.norm() <= 0.01)
        return;

    direction.normTo(1);
    double baseAngle = direction.getAngle();
    double spreadRadians = ((launch.Track || launch.OperationType == 1 || launch.OperationType == 2) ? 12.0 : 20.0) / 180.0 * M_PI;
    Role* primaryTarget = findNearestEnemy(launch.Attacker->Team, launch.Pos);
    double maxTravel = speed * std::max(1, launch.TotalFrame - launch.Frame) + TILE_W * 2;
    std::vector<Role*> alternateTargets;
    for (auto enemy : battle_roles_)
    {
        if (!enemy || enemy->Dead != 0 || enemy->Team == launch.Attacker->Team || enemy == primaryTarget)
            continue;
        if (EuclidDis(enemy->Pos, launch.Pos) > maxTravel)
            continue;
        alternateTargets.push_back(enemy);
    }
    std::sort(alternateTargets.begin(), alternateTargets.end(), [&](Role* lhs, Role* rhs) {
        auto lhsDir = lhs->Pos - launch.Pos;
        auto rhsDir = rhs->Pos - launch.Pos;
        double lhsAngle = lhsDir.norm() > 0.01 ? lhsDir.getAngle() : baseAngle;
        double rhsAngle = rhsDir.norm() > 0.01 ? rhsDir.getAngle() : baseAngle;
        double lhsDelta = std::abs(std::atan2(std::sin(lhsAngle - baseAngle), std::cos(lhsAngle - baseAngle)));
        double rhsDelta = std::abs(std::atan2(std::sin(rhsAngle - baseAngle), std::cos(rhsAngle - baseAngle)));
        if (lhsDelta != rhsDelta)
            return lhsDelta < rhsDelta;
        return EuclidDis(lhs->Pos, launch.Pos) < EuclidDis(rhs->Pos, launch.Pos);
    });

    for (int i = 0; i < projectileCount; ++i)
    {
        auto extra = launch;
        Role* assignedTarget = i < static_cast<int>(alternateTargets.size()) ? alternateTargets[i] : primaryTarget;
        double angle = baseAngle;
        if (assignedTarget)
        {
            auto targetDir = assignedTarget->Pos - launch.Pos;
            if (targetDir.norm() > 0.01)
                angle = targetDir.getAngle();
            extra.PreferredTarget = assignedTarget;
        }
        else if (projectileCount > 1)
        {
            double offset = (i - (projectileCount - 1) / 2.0) * spreadRadians;
            angle += offset;
        }

        extra.Velocity = { static_cast<float>(std::cos(angle)), static_cast<float>(std::sin(angle)) };
        extra.Velocity.normTo(speed);
        if (randomFrameRange > 0)
            extra.Frame = initialFrame + rand_.rand_int(randomFrameRange);
        else
            extra.Frame = initialFrame + i * frameStep;
        if (extra.Track || extra.OperationType == 0 || extra.OperationType == 3)
            extra.Track = 1;
        if (extra.OperationType == 0 || extra.OperationType == 3)
            extra.TotalFrame = std::max(extra.TotalFrame, 60);
        attack_effects_.push_back(extra);
    }
}

void BattleSceneHades::AI(Role* r)
{
    if (r->Dead == 0)
    {
        if (r->CoolDown == 0)
        {
            if (r->UsingMagic == nullptr)
            {
                {
                    auto selectMagic = [r](auto cmp)
                    {
                        auto v = r->getLearnedMagics();
                        auto hurt = r->getMagicPower(v[0]);
                        Magic* chooseMagic = v[0];
                        for (size_t i = 1; i < v.size(); ++i)
                        {
                            auto m = v[i];
                            double h = r->getMagicPower(m);
                            if (cmp(h, hurt))
                            {
                                hurt = h;
                                chooseMagic = m;
                            }
                        }
                        return chooseMagic;
                    };
                    bool isUltimate = r->MP >= r->MaxMP;
                    r->UsingMagic = isUltimate ? selectMagic(std::greater<int>{}) : selectMagic(std::less<int>{});
                    if (isUltimate)
                    {
                        ultCasters_.insert(r);
                        addFloatingText(r, std::string(r->UsingMagic->Name), {255, 215, 0, 255}, EMPHASIS_TEXT_SIZE);
                    }
                }
            }
            // Use per-agent target assignment: rear agents flank, front agents go nearest
            auto r0 = assignFlankTarget(r);
            if (!r0) r0 = findNearestEnemy(r->Team, r->Pos);
            if (r0)
            {
                r->RealTowards = r0->Pos - r->Pos;
                //r->FaceTowards = realTowardsToFaceTowards(r->RealTowards);
                r->RealTowards.normTo(1);
                int dis = TILE_W * 3;
                if (r->UsingMagic)
                {
                    if (r->UsingMagic->AttackAreaType == 3) { dis = 180; }
                    if (r->UsingMagic->AttackAreaType == 1 || r->UsingMagic->AttackAreaType == 2)
                    {
                        // 10 pixel to allow for some error
                        dis = std::min(
                            calcProjectileReach(r->UsingMagic->SelectDistance, TILE_W * 2) - 10, 240);
                    }
                }
                double speed = r->Speed / 30.0;
                if (EuclidDis(r->Pos, r0->Pos) > dis)
                {
                    auto p = r->Pos + speed * r->RealTowards;
                    if (canWalk90(p, r) && r->FindingWay == 0)
                    {
                        if (rand_.rand() < 0.25 && r->UsingMagic)
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
                            r->Velocity = speed * r->RealTowards;
                        }
                    }
                    else if (r->Velocity.norm() < 0.1)
                    {
                        // A* pathfinding with caching
                        auto& path_info = paths_[r];
                        path_info.frames_since_update++;

                        // Recompute path if needed
                        if (path_info.target != r0 || path_info.frames_since_update > 30 || path_info.waypoints.empty()) {
                            auto start45 = pos90To45(r->Pos.x, r->Pos.y);
                            auto goal45 = pos90To45(r0->Pos.x, r0->Pos.y);
                            auto path45 = findPath(start45, goal45);
                            path_info.waypoints.clear();
                            for (auto& p : path45) {
                                path_info.waypoints.push_back(pos45To90(p.x, p.y));
                            }
                            path_info.current_waypoint = 0;
                            path_info.frames_since_update = 0;
                            path_info.target = r0;
                        }

                        // Follow path
                        Pointf best_dir = r0->Pos - r->Pos;
                        if (path_info.current_waypoint < path_info.waypoints.size()) {
                            Pointf wp = path_info.waypoints[path_info.current_waypoint];
                            if (EuclidDis(r->Pos, wp) < 40) {
                                path_info.current_waypoint++;
                            }
                            if (path_info.current_waypoint < path_info.waypoints.size()) {
                                wp = path_info.waypoints[path_info.current_waypoint];
                                best_dir = wp - r->Pos;
                            }
                        }
                        best_dir.normTo(1);

                        r->FindingWay = 1;
                        r->RealTowards = best_dir;
                        r->Velocity = best_dir * speed;

                        if (rand_.rand() < 0.25 && r->UsingMagic) {
                            r->OperationType = 3;
                        } else {
                            r->OperationType = -1;
                        }

                        if (r->OperationType == 3) {
                            r->CoolDown = calCoolDown(r->UsingMagic->MagicType, r->OperationType, r);
                            r->ActFrame = 0;
                            r->HaveAction = 1;
                        }
                    }
                }
                else
                {
                    r->FindingWay = 0;
                    if (r->UsingMagic)
                    {
                        //点攻击疯狗咬即可
                        if (r->UsingMagic->AttackAreaType == 0 || rand_.rand() < 0.75 && r->UsingMagic->AttackAreaType != 0)
                        {
                            //attack
                            auto m = r->UsingMagic;
                            if (m)
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
                            if (rand_.rand() < 0.25)
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
                        if (!r->HaveAction)
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

    // Print pathfinding stats every 300 frames
    static int print_counter = 0;
    if (++print_counter >= 300) {
        print_counter = 0;
        for (auto& [role, info] : paths_) {
            if (role->Team == 0 && !role->Dead) {
                std::print("Role {} {}: following={} sliding={} stuck={} waypoints={}/{}\n",
                    role->ID, role->Name.c_str(), info.frames_following, info.frames_sliding,
                    info.frames_stuck, info.current_waypoint, (int)info.waypoints.size());
                info.frames_following = 0;
                info.frames_sliding = 0;
                info.frames_stuck = 0;
            }
        }
    }
}

void BattleSceneHades::onPressedCancel()
{
}

BattleLogData BattleSceneHades::buildBattleLogData() const
{
    BattleLogData data;
    data.title = "本场战斗日志";
    if (result_ == 0)
    {
        data.resultText = "我方胜利";
    }
    else if (result_ == 1)
    {
        data.resultText = "敌方胜利";
    }
    else
    {
        data.resultText = "战斗结束";
    }

    data.totalFrames = tracker_.getBattleEndFrame();

    auto appendRoleRows = [&](const std::deque<Role>& roles, int team, std::vector<BattleLogRoleRow>& rows)
    {
        auto& stats = tracker_.getStats();
        for (const auto& role : roles)
        {
            BattleLogRoleRow row;
            row.name = role.Name;
            row.team = team;
            row.cancelDmg = role.CancelDmg;
            row.hpRemaining = role.HP;
            row.maxHp = role.MaxHP;
            row.dead = role.Dead != 0 || role.HP <= 0;

            auto it = stats.find(role.ID);
            if (it != stats.end())
            {
                row.damageDealt = it->second.damageDealt;
                row.damageTaken = it->second.damageTaken;
                row.kills = it->second.kills;
            }
            rows.push_back(std::move(row));
        }

        std::sort(rows.begin(), rows.end(), [](const BattleLogRoleRow& a, const BattleLogRoleRow& b)
        {
            if (a.damageDealt != b.damageDealt) return a.damageDealt > b.damageDealt;
            if (a.kills != b.kills) return a.kills > b.kills;
            return a.name < b.name;
        });
    };

    appendRoleRows(friends_obj_, 0, data.allies);
    appendRoleRows(enemies_obj_, 1, data.enemies);

    auto pushSystemLine = [&](std::string text)
    {
        BattleLogLine line;
        line.tone = BattleLogTone::System;
        line.text = std::move(text);
        data.entries.push_back(std::move(line));
    };

    for (const auto& event : tracker_.getEvents())
    {
        BattleLogLine line;
        switch (event.type)
        {
        case BattleLogEventType::Damage:
            line.tone = teamToBattleLogTone(event.sourceTeam);
            appendBattleLogSegment(line, std::format("[{:>4}F] ", event.frame), BattleLogFieldTone::SystemAccent);
            appendBattleLogSegment(line, event.sourceName, teamToFieldTone(event.sourceTeam));
            if (!event.skillName.empty())
            {
                appendBattleLogSegment(line, " 施放 ");
                appendBattleLogSegment(line, event.skillName, BattleLogFieldTone::SkillName);
                appendBattleLogSegment(line, " 命中 ");
            }
            else
            {
                appendBattleLogSegment(line, " 攻击 ");
            }
            appendBattleLogSegment(line, event.targetName, teamToFieldTone(event.targetTeam));
            appendBattleLogSegment(line, " ，造成 ");
            appendBattleLogSegment(line, std::to_string(event.value), BattleLogFieldTone::DamageValue);
            appendBattleLogSegment(line, " 点伤害");
            data.entries.push_back(std::move(line));
            break;
        case BattleLogEventType::Kill:
            line.tone = BattleLogTone::System;
            appendBattleLogSegment(line, std::format("[{:>4}F] ", event.frame), BattleLogFieldTone::SystemAccent);
            appendBattleLogSegment(line, event.sourceName, teamToFieldTone(event.sourceTeam));
            appendBattleLogSegment(line, " 击杀了 ");
            appendBattleLogSegment(line, event.targetName, teamToFieldTone(event.targetTeam));
            data.entries.push_back(std::move(line));
            break;
        case BattleLogEventType::Death:
            line.tone = BattleLogTone::System;
            appendBattleLogSegment(line, std::format("[{:>4}F] ", event.frame), BattleLogFieldTone::SystemAccent);
            appendBattleLogSegment(line, event.targetName, teamToFieldTone(event.targetTeam));
            appendBattleLogSegment(line, " 倒下");
            data.entries.push_back(std::move(line));
            break;
        case BattleLogEventType::BattleEnd:
            pushSystemLine(std::format("[{:>4}F] {}", event.frame, data.resultText));
            break;
        }
    }

    if (data.entries.empty())
    {
        pushSystemLine("本场战斗没有产生可记录的伤害事件。");
    }

    constexpr int kMaxBattleLogEntries = 180;
    if ((int)data.entries.size() > kMaxBattleLogEntries)
    {
        data.omittedEntries = (int)data.entries.size() - kMaxBattleLogEntries;
        data.entries.erase(data.entries.begin(), data.entries.end() - kMaxBattleLogEntries);
        pushSystemLine(std::format("已省略较早的 {} 条记录。", data.omittedEntries));
    }

    return data;
}

void BattleSceneHades::renderExtraRoleInfo(Role* r, double x, double y)
{
    if (r == nullptr || r->Dead)
    {
        return;
    }

    auto& comboStates = KysChess::ChessCombo::getActiveStates();
    const KysChess::RoleComboState* comboState = nullptr;
    if (auto it = comboStates.find(r->ID); it != comboStates.end())
        comboState = &it->second;

    // 画个血条
    Color outline_color = { 0, 0, 0, 128 };
    constexpr int hpBarWidth = 24;
    constexpr int hpBarHeight = 3;
    constexpr int hpBarY = -60;

    auto renderOutline = [&](int bar_x, int bar_y, int width, int height, Color color, int alpha)
    {
        Rect top = { bar_x - 1, bar_y - 1, width + 2, 1 };
        Rect bottom = { bar_x - 1, bar_y + height, width + 2, 1 };
        Rect left = { bar_x - 1, bar_y, 1, height };
        Rect right = { bar_x + width, bar_y, 1, height };
        Engine::getInstance()->renderSquareTexture(&top, color, alpha);
        Engine::getInstance()->renderSquareTexture(&bottom, color, alpha);
        Engine::getInstance()->renderSquareTexture(&left, color, alpha);
        Engine::getInstance()->renderSquareTexture(&right, color, alpha);
    };

    auto renderBar = [&](int bar_y, int cur, int max, Color background_color, Color shadow_color)
    {
        constexpr auto width = 24;
        constexpr auto height = 3;
        int bar_x = int(x - width / 2);
        double perc = static_cast<double>(cur) / max;
        double alpha = 1;

        // Draw full background bar (max HP/MP) - shadow
        Rect r_bg = { bar_x, bar_y, width, height };
        Engine::getInstance()->renderSquareTexture(&r_bg, shadow_color, 128 * alpha);

        // Draw current HP/MP bar on top (percentage of max)
        Rect r1 = { bar_x, bar_y, int(perc * width), height };
        Engine::getInstance()->renderSquareTexture(&r1, background_color, 192 * alpha);

        // Draw outline
        renderOutline(bar_x, bar_y, width, height, outline_color, 128 * alpha);
    };

    Color background_color = { 0, 255, 0, 128 };    // 我方绿色
    Color shadow_color = { 64, 64, 64, 128 };       // 背景阴影
    if (r->Team == 1)
    {
        // 敌方红色
        background_color = { 255, 0, 0, 128 };
    }

    renderBar(y + hpBarY, r->HP, r->MaxHP, background_color, shadow_color);

    if (r->MaxHP > 0 && r->HP > 0)
    {
        int bar_x = int(x - hpBarWidth / 2);
        int bar_y = y + hpBarY;
        int hpFillWidth = std::clamp(static_cast<int>(std::round(hpBarWidth * (static_cast<double>(r->HP) / r->MaxHP))), 0, hpBarWidth);

        if (comboState && comboState->shield > 0)
        {
            int shieldWidth = std::clamp(static_cast<int>(std::round(hpBarWidth * (static_cast<double>(comboState->shield) / r->MaxHP))), 1, hpBarWidth);
            int visibleShieldWidth = std::min(shieldWidth, hpFillWidth);
            Rect shieldRect = { bar_x, bar_y, visibleShieldWidth, hpBarHeight };
            int shieldAlpha = std::clamp(90 + comboState->shield * 120 / std::max(1, r->MaxHP), 90, 180);
            Engine::getInstance()->renderSquareTexture(&shieldRect, { 70, 140, 255, 255 }, shieldAlpha);
        }

        bool hasDamageProtection = r->Invincible > 0 || (comboState && comboState->blockFirstHitsRemaining > 0);
        if (hasDamageProtection)
        {
            Color protectionColor = comboState && comboState->blockFirstHitsRemaining > 0
                ? Color{ 255, 220, 110, 255 }
                : Color{ 255, 170, 95, 255 };
            renderOutline(bar_x, bar_y, hpBarWidth, hpBarHeight, protectionColor, 220);
        }
    }

    Color mp_color = { 0, 0, 255, 128 };
    Color mp_shadow_color = { 64, 64, 64, 128 };
    renderBar(y - 56, r->MP, r->MaxMP, mp_color, mp_shadow_color);

    // Frozen bar
    if (r->Frozen > 0 && r->FrozenMax > 0)
    {
        constexpr auto width = 24;
        constexpr auto height = 3;
        Color frozen_color = { 200, 220, 255, 192 };
        int bar_x = int(x - width / 2);
        int bar_y = y - 52;
        double perc = static_cast<double>(r->Frozen) / r->FrozenMax;

        // Draw outline (same color as bar)
        Rect r_outline = { bar_x - 1, bar_y - 1, width + 2, height + 2 };
        Engine::getInstance()->renderSquareTexture(&r_outline, frozen_color, 128);

        // Draw current frozen bar
        Rect r_frozen = { bar_x, bar_y, int(perc * width), height };
        Engine::getInstance()->renderSquareTexture(&r_frozen, frozen_color, 192);

        std::string frames_text = std::to_string(r->Frozen);
        Font::getInstance()->draw(frames_text, 10, x - 5, y - 52, {255, 255, 255, 255});
    }


    if (positionSwapActive_ && r->Team == 0) {
        std::string coord = std::format("({},{})", r->X(), r->Y());
        int tw = Font::getTextDrawSize(coord);
        Font::getInstance()->draw(coord, 14, x - 5, y - 5, {255, 255, 100, 255});
    }
}

int BattleSceneHades::checkResult()
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

void BattleSceneHades::setRoleInitState(Role* r)
{
    BattleScene::setRoleInitState(r);
    if (r->Team == 0)
    {
        r->HP = r->MaxHP;
        r->MP = 0;
        r->PhysicalPower = (std::max)(r->PhysicalPower, 90);
    }
    else
    {
        r->HP = r->MaxHP;
        r->MP = 0;
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

Role* BattleSceneHades::findNearestEnemy(int team, Pointf p)
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

Role* BattleSceneHades::findFarthestEnemy(int team, Pointf p)
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

Role* BattleSceneHades::findRandomEnemy(int team)
{
    std::vector<Role*> candidates;
    for (auto r1 : battle_roles_)
    {
        if (r1->Dead == 0 && team != r1->Team)
            candidates.push_back(r1);
    }
    if (candidates.empty())
        return nullptr;
    return candidates[rand_.rand_int(static_cast<int>(candidates.size()))];
}

Role* BattleSceneHades::findWeakestVulnerableEnemy(int team)
{
    Role* target = nullptr;
    double bestScore = std::numeric_limits<double>::max();
    for (auto r1 : battle_roles_)
    {
        if (r1->Dead != 0 || team == r1->Team || r1->Invincible > 0)
            continue;

        double effectiveHp = r1->MaxHP + r1->Defence * BLINK_WEAK_TARGET_DEF_WEIGHT;
        if (effectiveHp < bestScore)
        {
            bestScore = effectiveHp;
            target = r1;
        }
    }
    return target;
}

//前摇
int BattleSceneHades::calCast(int act_type, int operation_type, Role* r)
{
    int v[4] = { 25, 30, 20, 25 };
    if (operation_type >= 0 && operation_type <= 3)
    {
        return v[operation_type];
    }
    return 0;
}

//冷却减去前摇就是后摇
//需注意攻击判定可能仍然存在，严格来说攻击判定存在的时间加上前摇应小于冷却
int BattleSceneHades::calCoolDown(int act_type, int operation_type, Role* r)
{
    int i = r->getActProperty(act_type);
    int v[4] = { 90 - i / 2, 210 - i, 115 - i / 2, 35 };
    int min_v[4] = { 67, 80, 75, 35 };
    if (operation_type >= 0 && operation_type <= 3)
    {
        int c = std::max(min_v[operation_type], v[operation_type]);
        int spd = std::min(100, r->Speed);
        c = c * (1.0 - 0.334 * spd / 100.0);
        c = std::max(calCast(act_type, operation_type, r) + 2, c);
        // CDR from combos/neigong — clamp so cooldown never drops below cast time
        auto& cs = KysChess::ChessCombo::getMutableStates();
        auto it = cs.find(r->ID);
        if (it != cs.end() && it->second.cdrPct > 0)
        {
            c = static_cast<int>(c * (1.0 - it->second.cdrPct / 100.0));
            c = std::max(calCast(act_type, operation_type, r) + 2, c);
        }
        return c;
    }
    else
    {
        return 0;
    }
}

void BattleSceneHades::defaultMagicEffect(AttackEffect& ae, Role* r)
{
    if (ae.NoHurt)
    {
        return;
    }
    double hurt;
    bool reflectToAttacker = false;
    bool attackerIgnoreDefense = false;
    bool executed = false;
    bool rangedProjectile = ae.UsingHiddenWeapon != nullptr || ae.OperationType == 2;
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
    hurt *= 1 - 0.3 * ae.Frame / ae.TotalFrame;    //距离衰减
    //角度
    auto atk_dir = ae.Pos - r->Pos;
    auto angle = acos((atk_dir.x * r->RealTowards.x + atk_dir.y * r->RealTowards.y) / atk_dir.norm() / r->RealTowards.norm());
    if (angle >= M_PI * 0.25 && angle < M_PI * 0.75)
    {
        hurt *= 1.2;
    }
    else if (angle >= M_PI * 0.75)
    {
        hurt *= 1.5;
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
    }
    if (ae.OperationType == 3)
    {
        hurt /= 1.5;
        applyFrozen(r, 5);
    }
    //击退
    auto v = r->Pos - ae.Attacker->Pos;
    double pushStr = (ae.OperationType == 2) ? 0.5 : 2.0;
    v.normTo(pushStr);
    r->Velocity += v;
    double pushCap = (ae.OperationType == 2) ? 1.5 : 3.0;
    if (r->Velocity.norm() > pushCap)
    {
        r->Velocity.normTo(pushCap);
    }
    r->HurtFrame = 1;    //相当于无敌时间

    //武功类型特殊效果
    if (ae.UsingMagic)
    {
        // 简化
        int actType = ae.UsingMagic->MagicType;
        int actDiff = ae.Attacker->getActProperty(actType) - r->getActProperty(actType);
        hurt *= 1 + std::clamp((actDiff / 200.0), -0.25, 0.25);
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
    ae.Attacker->ExpGot += hurt / 2;    // deprecated: kept for compat but unused

    // === Combo trigger effects ===
    {
        auto& cs = KysChess::ChessCombo::getMutableStates();

        // Attacker effects
        auto ait = cs.find(ae.Attacker->ID);
        if (ait != cs.end())
        {
            auto& as = ait->second;

            // Skill damage bonus
            if (ae.UsingMagic && as.skillDmgPct > 0)
                hurt *= (1.0 + as.skillDmgPct / 100.0);

            // Flat damage increase
            hurt += as.flatDmgIncrease;

            // Triggered effect damage modifiers
            for (auto& te : as.triggeredEffects)
            {
                bool trigActive = false;
                if (te.trigger == KysChess::Trigger::WhileLowHP)
                    trigActive = ae.Attacker->HP * 100 / std::max(1, ae.Attacker->MaxHP) < te.triggerValue;
                else if (te.trigger == KysChess::Trigger::LastAlive)
                    trigActive = as.lastAliveFlag;
                else if (te.trigger == KysChess::Trigger::AllyLowHPBurst)
                    trigActive = as.triggerTimers.count(te.trigger) && as.triggerTimers.at(te.trigger) > 0;

                if (trigActive)
                {
                    if (te.type == KysChess::EffectType::PctATK)
                        hurt *= (1.0 + te.value / 100.0);
                }
            }

            // Crit
            bool critted = false;
            if (as.dodgedLast && as.dodgeThenCrit)
            {
                critted = true;
                as.dodgedLast = false;
            }
            if (!critted && as.critChancePct > 0 && rand_.rand() * 100 < as.critChancePct)
                critted = true;
            if (critted)
            {
                hurt *= as.critMultiplier / 100.0;
                addFloatingText(ae.Attacker, "暴击", {255, 100, 0, 255}, STATUS_TEXT_SIZE);
            }

            // Every-Nth-hit double
            for (int n : as.everyNthDoubles)
            {
                auto& counter = as.everyNthCounters[n];
                counter++;
                if (counter >= n)
                {
                    hurt *= 2.0;
                    counter = 0;
                }
            }

            // Ramping damage: each hit stacks bonus damage
            for (size_t i = 0; i < as.rampings.size(); i++)
            {
                auto& ramp = as.rampings[i];
                as.rampingStacks[i] = std::min(as.rampingStacks[i] + 1, ramp.maxStacks);
                as.rampingIdleTimers[i] = 90;
                hurt *= (1.0 + as.rampingStacks[i] * ramp.pctPerStack / 100.0);
            }

            // MP on hit
            if (as.mpOnHit > 0)
                changeRoleMP(ae.Attacker, as.mpOnHit);

            // MP drain
            if (as.mpDrain > 0)
            {
                int drain = std::min(as.mpDrain, (int)r->MP);
                r->MP -= drain;
                changeRoleMP(ae.Attacker, drain);
            }

            // Poison application on target
            if (as.poisonDOTPct > 0)
            {
                auto dit = cs.find(r->ID);
                if (dit != cs.end())
                {
                    int newDmg = r->HP * as.poisonDOTPct / 100;
                    int oldDmg = r->HP * dit->second.poisonTickDmg / 100;
                    if (newDmg > oldDmg)
                    {
                        dit->second.poisonTimer = as.poisonDuration;
                        dit->second.poisonTickDmg = as.poisonDOTPct;
                    }
                }
                else
                {
                    auto& ds = cs[r->ID];
                    ds.poisonTimer = as.poisonDuration;
                    ds.poisonTickDmg = as.poisonDOTPct;
                }
            }

            // Stun (legacy)
            if (as.stunChancePct > 0 && rand_.rand() * 100 < as.stunChancePct)
                applyFrozen(r, as.stunFrames);

            // OnHit triggered effects
            for (auto& eff : as.triggeredEffects)
            {
                if (eff.trigger != KysChess::Trigger::OnHit)
                    continue;

                if (eff.type == KysChess::EffectType::ArmorPen)
                {
                    // Applied in calMagicHurt
                }
                else if (eff.type == KysChess::EffectType::Stun && rand_.rand() * 100 < eff.triggerValue)
                {
                    applyFrozen(r, eff.value);
                }
            }

            // Knockback (extra)
            if (as.knockbackChancePct > 0 && rand_.rand() * 100 < as.knockbackChancePct)
            {
                auto kb = r->Pos - ae.Attacker->Pos;
                kb.normTo(5);
                r->Velocity += kb;
            }

            if (as.offensiveCharmChancePct > 0 && as.charmCDRAmountPct > 0
                && rand_.rand() * 100 < as.offensiveCharmChancePct)
                increaseCooldown(r, as.charmCDRAmountPct);
        }

        // Defender effects
        auto dit = cs.find(r->ID);
        if (dit != cs.end())
        {
            auto& ds = dit->second;
            bool invincibleBeforeHit = r->Invincible > 0;

            if (!attackerIgnoreDefense)
            {
                // Flat damage reduction
                hurt -= ds.flatDmgReduction;

                // Percent damage reduction
                if (ds.dmgReductionPct > 0)
                    hurt *= (1.0 - ds.dmgReductionPct / 100.0);

                // Triggered defender effects (DmgReductionPct with triggers)
                for (auto& te : ds.triggeredEffects)
                {
                    bool trigActive = false;
                    if (te.trigger == KysChess::Trigger::WhileLowHP)
                        trigActive = r->HP * 100 / std::max(1, r->MaxHP) < te.triggerValue;
                    else if (te.trigger == KysChess::Trigger::LastAlive)
                        trigActive = ds.lastAliveFlag;
                    if (trigActive && te.type == KysChess::EffectType::DmgReductionPct)
                        hurt *= (1.0 - te.value / 100.0);
                }

                // Adaptation: reduce damage from repeated attacker
                for (size_t i = 0; i < ds.adaptations.size(); i++)
                {
                    auto& adapt = ds.adaptations[i];
                    int& stacks = ds.adaptationStacks[i][ae.Attacker->ID];
                    stacks = std::min(stacks + 1, adapt.maxStacks);
                    hurt *= (1.0 - stacks * adapt.pctPerStack / 100.0);
                }
            }

            // Poison damage amp (target is poisoned)
            if (ds.poisonTimer > 0 && ait != cs.end() && ait->second.poisonDmgAmpPct > 0)
                hurt *= (1.0 + ait->second.poisonDmgAmpPct / 100.0);

            if (ds.charmCDRChancePct > 0 && rand_.rand() * 100 < ds.charmCDRChancePct)
                increaseCooldown(ae.Attacker, ds.charmCDRAmountPct);

            // OnBeingHit triggered effects
            for (auto& eff : ds.triggeredEffects)
            {
                if (eff.trigger != KysChess::Trigger::OnBeingHit)
                    continue;

                if (eff.type == KysChess::EffectType::Stun && rand_.rand() * 100 < eff.triggerValue)
                {
                    applyFrozen(ae.Attacker, eff.value);
                }
            }

            if (rangedProjectile && ds.projectileReflectPct > 0
                && rand_.rand() * 100 < ds.projectileReflectPct)
            {
                reflectToAttacker = true;
                addFloatingText(r, "弹反", {100, 220, 255, 255}, STATUS_TEXT_SIZE);
            }

            if (!reflectToAttacker && ds.shield > 0)
            {
                int shieldBefore = ds.shield;
                int absorbed = std::min(ds.shield, (int)hurt);
                ds.shield -= absorbed;
                hurt -= absorbed;
                if (shieldBefore > 0 && ds.shield == 0 && ds.shieldExplosionPct > 0)
                {
                    int explosionDmg = std::max(1, shieldBefore * ds.shieldExplosionPct / 100);
                    spawnAreaImpactProjectiles(r, r, 9, 9, KysChess::EFT_SHIELD_BLAST, explosionDmg);
                }
            }

            if (!reflectToAttacker && ait != cs.end())
            {
                auto& as = ait->second;
                for (auto& eff : as.triggeredEffects)
                {
                    if (eff.trigger != KysChess::Trigger::OnHit || eff.type != KysChess::EffectType::Execute)
                        continue;

                    int executeChance = eff.triggerValue;
                    int executeThreshold = eff.value;
                    if (executeChance <= 0 || executeThreshold <= 0 || hurt <= 0)
                        continue;

                    int projectedHP = r->HP - r->HurtThisFrame;
                    if (ae.UsingHiddenWeapon || ae.UsingMagic->HurtType == 0)
                        projectedHP -= static_cast<int>(hurt);
                    if (projectedHP * 100 < r->MaxHP * executeThreshold
                        && rand_.rand() * 100 < executeChance)
                    {
                        executed = true;
                        break;
                    }
                }
            }

            if (!executed && invincibleBeforeHit)
            {
                hurt = 0;
            }

            if (!executed && !reflectToAttacker && ds.blockChancePct > 0 && rand_.rand() * 100 < ds.blockChancePct)
            {
                hurt = 0;
                addRoleEffect(r, KysChess::EFT_BLOCK, ROLE_STATUS_EFT_FRAMES);
            }

            if (!executed && !reflectToAttacker && hurt > 0 && ds.blockFirstHitsRemaining > 0)
            {
                hurt = 0;
                ds.blockFirstHitsRemaining--;
                addRoleEffect(r, KysChess::EFT_BLOCK, ROLE_STATUS_EFT_FRAMES);
            }

            if (!reflectToAttacker && ae.UsingMagic && ds.skillReflectPct > 0)
                ae.Attacker->HurtThisFrame += static_cast<int>(hurt * ds.skillReflectPct / 100.0);
        }

        if (!reflectToAttacker && ait != cs.end())
        {
            auto& as = ait->second;

            if (hurt > 0 && as.bleedChancePct > 0 && rand_.rand() * 100 < as.bleedChancePct)
            {
                auto& ds = cs[r->ID];
                ds.bleedStacks = std::min(ds.bleedStacks + 1, as.bleedMaxStacks);
                if (ds.bleedTimer <= 0)
                    ds.bleedTimer = 10;
                ds.bleedPersistFlag = as.bleedPersist;
            }

            for (auto& eff : as.triggeredEffects)
            {
                if (eff.trigger != KysChess::Trigger::OnHit)
                    continue;

                if (eff.type == KysChess::EffectType::MPBlock)
                {
                    int chance = eff.triggerValue;
                    int frames = eff.value;
                    if (chance > 0 && frames > 0 && rand_.rand() * 100 < chance)
                        cs[r->ID].mpBlockTimer = std::max(cs[r->ID].mpBlockTimer, frames);
                }
            }
        }

        // Track last attacker for kill attribution
        r->LastAttacker = ae.Attacker;
        if (reflectToAttacker)
            ae.Attacker->LastAttacker = r;
    }

    //扣HP或MP
    Role* actualSource = reflectToAttacker ? r : ae.Attacker;
    Role* actualTarget = reflectToAttacker ? ae.Attacker : r;
    if (ae.UsingHiddenWeapon || ae.UsingMagic->HurtType == 0)
    {
        actualTarget->HurtThisFrame += hurt;
        if (!reflectToAttacker && executed)
        {
            actualTarget->HurtThisFrame = std::max(actualTarget->HurtThisFrame, actualTarget->HP);
            execution_popup_roles_.insert(actualTarget->ID);
        }
        std::string skillName = ae.UsingMagic ? std::string(ae.UsingMagic->Name) : "";
        tracker_.recordDamage(actualSource, actualTarget, (int)hurt, skillName, current_frame_);
    }
    if (ae.UsingMagic && ae.UsingMagic->HurtType == 1)
    {
        actualTarget->MP -= hurt;
        changeRoleMP(actualSource, hurt * 0.8);
        TextEffect te;
        te.set(std::to_string(int(-hurt)), { 160, 32, 240, 255 }, actualTarget);
        text_effects_.push_back(std::move(te));
    }
    //LOG("{} attack {} with {} as {}, hurt {}\n", ae.Attacker->Name, r->Name, ae.UsingMagic->Name, ae.OperationType, int(hurt));
}

void BattleSceneHades::applyScriptedAttackEffect(AttackEffect& ae, Role* r)
{
    if (!r)
        return;

    if (ae.ScriptedStunFrames > 0)
        applyFrozen(r, ae.ScriptedStunFrames);

    if (ae.Through == 0)
    {
        ae.NoHurt = 1;
        ae.Frame = std::max(ae.TotalFrame - 15, ae.Frame);
    }

    r->LastAttacker = ae.Attacker;
    if (ae.ScriptedDamage > 0)
    {
        r->HurtThisFrame += ae.ScriptedDamage;
        tracker_.recordDamage(ae.Attacker, r, ae.ScriptedDamage, "", current_frame_);
    }
}

int BattleSceneHades::calRolePic(Role* r, int style, int frame)
{
    if (style < 0 || style >= 5)
    {
        style = resolveFightStyle(r, -1);
        if (style >= 0)
        {
            return r->FightFrame[style] * r->FaceTowards;
        }
    }
    else
    {
        style = resolveFightStyle(r, style);
    }
    if (style < 0)
    {
        return r->FaceTowards;
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

void BattleSceneHades::makeSpecialMagicEffect()
{
}

int BattleSceneHades::calMagicHurt(Role* r1, Role* r2, Magic* magic, int dis)
{
    int power = r1->getMagicPower(magic);
    double attack = r1->Attack + power / 3.0;
    double defence = r2->Defence;
    // Armor penetration: reduce effective defence
    auto& cs = KysChess::ChessCombo::getMutableStates();
    auto it = cs.find(r1->ID);
    if (it != cs.end())
    {
        auto& as = it->second;
        // Legacy armor pen
        if (as.armorPenChancePct > 0 && rand_.rand() * 100 < as.armorPenChancePct)
            defence *= (1.0 - as.armorPenPct / 100.0);
        // New unified armor pen
        for (auto& eff : as.triggeredEffects)
        {
            if (eff.trigger == KysChess::Trigger::OnHit && eff.type == KysChess::EffectType::ArmorPen)
            {
                if (rand_.rand() * 100 < eff.triggerValue)
                    defence *= (1.0 - eff.value / 100.0);
            }
        }
    }
    if (attack + defence <= 0) return 1;
    int v = static_cast<int>(attack * attack / (attack + defence) / 4.0);
    v += rand_.rand_int(10) - rand_.rand_int(10);
    if (v < 1) v = 1;
    return v;
}

std::vector<Point> BattleSceneHades::findPath(Point start45, Point goal45)
{
    MapSquareInt dis_layer;
    dis_layer.resize(COORD_COUNT);
    calDistanceLayer(goal45.x, goal45.y, dis_layer, 64);

    std::vector<Point> path;
    Point current = start45;
    path.push_back(current);

    while (current.x != goal45.x || current.y != goal45.y) {
        int current_dis = dis_layer.data(current.x, current.y);
        if (current_dis > 64) break;

        Point best = current;
        int best_dis = current_dis;
        int dx[] = {-1, 1, 0, 0};
        int dy[] = {0, 0, -1, 1};

        for (int i = 0; i < 4; i++) {
            int nx = current.x + dx[i];
            int ny = current.y + dy[i];
            if (!isOutLine(nx, ny) && canWalk(nx, ny)) {
                int nd = dis_layer.data(nx, ny);
                if (nd < best_dis) {
                    best = {nx, ny};
                    best_dis = nd;
                }
            }
        }

        if (best.x == current.x && best.y == current.y) break;
        current = best;
        path.push_back(current);
    }

    return path;
}

std::vector<Pointf> BattleSceneHades::smoothPath(const std::vector<Point>& path45)
{
    if (path45.size() <= 2) {
        std::vector<Pointf> result;
        for (auto& p : path45) {
            result.push_back(pos45To90(p.x, p.y));
        }
        return result;
    }

    std::vector<Pointf> smoothed;
    smoothed.push_back(pos45To90(path45[0].x, path45[0].y));

    int i = 0;
    while (i < path45.size() - 1) {
        int j = path45.size() - 1;
        while (j > i + 1) {
            if (canWalk(path45[j].x, path45[j].y)) {
                bool clear = true;
                int steps = std::max(abs(path45[j].x - path45[i].x), abs(path45[j].y - path45[i].y));
                for (int k = 1; k < steps; k++) {
                    int mx = path45[i].x + (path45[j].x - path45[i].x) * k / steps;
                    int my = path45[i].y + (path45[j].y - path45[i].y) * k / steps;
                    if (!canWalk(mx, my)) {
                        clear = false;
                        break;
                    }
                }
                if (clear) break;
            }
            j--;
        }
        i = j;
        smoothed.push_back(pos45To90(path45[i].x, path45[i].y));
    }

    return smoothed;
}

Role* BattleSceneHades::assignFlankTarget(Role* r)
{
    // Instead of always targeting nearest enemy, rear-line agents
    // target the least-targeted enemy to create flanking behavior.
    // Count how many allies are already targeting each enemy.
    struct EnemyInfo {
        Role* enemy;
        double dist;
        int targeters;
    };
    std::vector<EnemyInfo> enemies;
    for (auto e : battle_roles_) {
        if (e->Team != r->Team && !e->Dead) {
            enemies.push_back({e, EuclidDis(r->Pos, e->Pos), 0});
        }
    }
    if (enemies.empty()) return nullptr;

    // Count allies closer to each enemy than we are (they are "ahead" of us)
    for (auto ally : battle_roles_) {
        if (ally == r || ally->Team != r->Team || ally->Dead) continue;
        for (auto& ei : enemies) {
            if (EuclidDis(ally->Pos, ei.enemy->Pos) < ei.dist) {
                ei.targeters++;
            }
        }
    }

    // Score: prefer enemies with fewer targeters, tie-break by distance
    Role* best = enemies[0].enemy;
    double best_score = 1e9;
    for (auto& ei : enemies) {
        // Each targeter adds 120 pixels of virtual distance
        double score = ei.dist + ei.targeters * 120.0;
        if (score < best_score) {
            best_score = score;
            best = ei.enemy;
        }
    }
    return best;
}
