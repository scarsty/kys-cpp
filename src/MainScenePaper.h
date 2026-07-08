#pragma once
#include "MainScene.h"
#include "PaperSky.h"

class MainScenePaper : public MainScene
{
public:
    MainScenePaper();

    virtual void draw() override;
    virtual void dealEvent(EngineEvent& e) override;
    virtual void onEntrance() override;

private:
    Pointf pos45To90(int x, int y) const;
    bool inCameraFront(const std::vector<Pointf>& points);
    bool isMountainTexture(int pic) const;
    void renderTexture3D(Texture* texture, const std::vector<Pointf>& world, const std::vector<FPoint>& src);
    void renderGroundPatch3D(Texture* texture, float world_x0, float world_y0, float world_x1, float world_y1, float src_w, float src_h, int mesh_x, int mesh_y);

    Pointf camera_pos_ = { 0, 0, 500 };
    Pointf camera_focus_ = { 0, 0, 0 };
    float camera_yaw_ = 0.0f;
    float camera_height_ = 150.0f;
    PaperSky paper_sky_;
};
