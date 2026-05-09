#pragma once

#include "Point.h"
#include "Types.h"

class BattleSceneMapState
{
public:
    void initialize(int coordCount);
    void loadBattlefield(int battlefieldId);
    void makeEarthTexture(int renderCenterX, int renderCenterY);

    bool isOutLine(int x, int y);
    bool isBuilding(int x, int y);
    bool isWater(int x, int y);
    bool canWalk45(int x, int y);
    Pointf pos45To90(int x, int y) const;
    Point pos90To45(double x, double y) const;

    MapSquareInt& earthLayer() { return earthLayer_; }
    MapSquareInt& buildingLayer() { return buildingLayer_; }
    const MapSquareInt& earthLayer() const { return earthLayer_; }
    const MapSquareInt& buildingLayer() const { return buildingLayer_; }

private:
    Point positionOnWholeEarth(int x, int y, int renderCenterX, int renderCenterY) const;

    int coordCount_ = 0;
    int battlefieldId_ = -1;
    MapSquareInt earthLayer_;
    MapSquareInt buildingLayer_;
};
