#pragma once

#include "Camera.h"
#include "Engine.h"
#include "PaperSky.h"
#include "Types.h"
#include <functional>

struct TextureWarpper;
struct Role;

struct PaperRenderSprite
{
    Texture* texture = nullptr;
    TextureWarpper* tex = nullptr;
    Pointf anchor;
    std::vector<Pointf> world;
    std::vector<FPoint> source;
    Color color{ 255, 255, 255, 255 };
    uint8_t alpha = 255;
    float depth = 0;
    int turn = 1;
    int rotation = 0;
    bool use_perspective_mesh = false;
    bool face_camera = false;
    int order = 0;
};

struct PaperGroundTargets
{
    Texture* base_texture = nullptr;
    Texture* texture = nullptr;
    int base_content_width = 0;
    int base_content_height = 0;
    int texture_width = 0;
    int texture_height = 0;
    int base_width = 0;
    int base_height = 0;
    int texture_size = 0;
};

struct PaperRoleInfoAnchor
{
    Role* role = nullptr;
    Pointf position;
};

struct PaperTextEffect
{
    Pointf position;
    std::string text;
    int size = 30;
    int rise = 0;
    Color color;
};

struct PaperScene
{
    MapSquareInt* earth_layer = nullptr;
    MapSquareInt* building_layer = nullptr;
    int battle_field_id = 0;
    int coordinate_count = 0;
    int tile_width = 0;
    int tile_height = 0;
    std::function<Pointf(int, int)> to_world;
    std::function<bool(int)> is_wall_tile;
};

struct PaperFrame
{
    Camera* camera = nullptr;
    std::vector<PaperRenderSprite> sprites;
    std::vector<PaperRoleInfoAnchor> role_info_anchors;
    std::vector<PaperTextEffect> text_effects;
    const Pointf* locked_position = nullptr;
    int current_frame = 0;
    int tile_width = 0;
    int tile_width_base = 0;
    int lock_texture_id = 0;
    float role_info_top_gap = 0;
    int scene_width = 0;
    int scene_height = 0;
    std::function<void(Role*, int, int)> render_role_info;
};

class PaperPresentation
{
public:
    void initialize();
    void renderGroundMesh(Engine* engine, const PaperGroundTargets& targets,
        Camera& camera, int coordinate_count, int tile_width) const;
    void appendMapSprites(Engine* engine, Camera& camera, MapSquareInt& building_layer,
        int coordinate_count, const std::function<Pointf(int, int)>& to_world,
        const std::function<bool(int)>& is_wall_tile, std::vector<PaperRenderSprite>& sprites) const;
    void renderSprites(Engine* engine, Camera& camera, std::vector<PaperRenderSprite>& sprites) const;
    void renderLockMarker(Engine* engine, Camera& camera, const Pointf& position,
        int current_frame, int tile_width, int texture_id) const;
    void renderRoleInfoAnchors(Camera& camera, const std::vector<PaperRoleInfoAnchor>& anchors,
        int tile_width, int tile_width_base, float top_gap,
        const std::function<void(Role*, int, int)>& render_info) const;
    void renderTextEffects(Engine* engine, Camera& camera, const std::vector<PaperTextEffect>& effects,
        int tile_width, int scene_width, int scene_height) const;
    void initializeScene(Engine* engine, const PaperScene& scene);
    void renderScene(Engine* engine, PaperFrame& frame);
    void renderSky(Engine* engine, int viewport_width, int viewport_height,
        const Pointf& camera_pos, const Pointf& camera_center);

private:
    PaperGroundTargets createGroundTargets(Engine* engine, int coordinate_count,
        int tile_width, int tile_height, int requested_extension);
    void bakeGround(Engine* engine);
    PaperSky sky_;
    PaperGroundTargets ground_targets_;
    PaperScene scene_;
};