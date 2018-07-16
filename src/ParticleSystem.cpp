#include "ParticleSystem.h"
#include <assert.h>
#include <string>

inline float Deg2Rad(float a)
{
    return a * 0.01745329252f;
}

inline float Rad2Deg(float a)
{
    return a * 57.29577951f;
}

inline float clampf(float value, float min_inclusive, float max_inclusive)
{
    if (min_inclusive > max_inclusive)
    {
        std::swap(min_inclusive, max_inclusive);
    }
    return value < min_inclusive ? min_inclusive : value < max_inclusive ? value : max_inclusive;
}

inline void normalize_point(float x, float y, Pointf* out)
{
    float n = x * x + y * y;
    // Already normalized.
    if (n == 1.0f)
    {
        return;
    }

    n = sqrt(n);
    // Too close to zero.
    if (n < 1e-5)
    {
        return;
    }

    n = 1.0f / n;
    out->x = x * n;
    out->y = y * n;
}

/**
A more effect random number getter function, get from ejoy2d.
*/
inline static float RANDOM_M11(unsigned int* seed)
{
    *seed = *seed * 134775813 + 1;
    union
    {
        uint32_t d;
        float f;
    } u;
    u.d = (((uint32_t)(*seed) & 0x7fff) << 8) | 0x40000000;
    return u.f - 3.0f;
}

ParticleSystem::ParticleSystem()
    : _isBlendAdditive(false)
    , _isAutoRemoveOnFinish(false)
    , _plistFile("")
    , _elapsed(0)
    , _configName("")
    , _emitCounter(0)
      //, _batchNode(nullptr)
    , _atlasIndex(0)
    , _transformSystemDirty(false)
    , _allocatedParticles(0)
    , _isActive(true)
    , _particleCount(0)
    , _duration(0)
    , _life(0)
    , _lifeVar(0)
    , _angle(0)
    , _angleVar(0)
    , _emitterMode(Mode::GRAVITY)
    , _startSize(0)
    , _startSizeVar(0)
    , _endSize(0)
    , _endSizeVar(0)
    , _startSpin(0)
    , _startSpinVar(0)
    , _endSpin(0)
    , _endSpinVar(0)
    , _emissionRate(0)
    , _totalParticles(0)
    , _texture(nullptr)
      //, _blendFunc(BlendFunc::ALPHA_PREMULTIPLIED)
    , _opacityModifyRGB(false)
    , _yCoordFlipped(1)
      //, _positionType(PositionType::FREE)
    , _paused(false)
    , _sourcePositionCompatible(true)    // In the furture this member's default value maybe false or be removed.
{
    modeA.gravity = { 0, 0 };
    modeA.speed = 0;
    modeA.speedVar = 0;
    modeA.tangentialAccel = 0;
    modeA.tangentialAccelVar = 0;
    modeA.radialAccel = 0;
    modeA.radialAccelVar = 0;
    modeA.rotationIsDir = false;
    modeB.startRadius = 0;
    modeB.startRadiusVar = 0;
    modeB.endRadius = 0;
    modeB.endRadiusVar = 0;
    modeB.rotatePerSecond = 0;
    modeB.rotatePerSecondVar = 0;
}

// implementation ParticleSystem
bool ParticleSystem::init()
{
    return initWithTotalParticles(150);
}

bool ParticleSystem::initWithTotalParticles(int numberOfParticles)
{
    _totalParticles = numberOfParticles;

    _particleData.resize(numberOfParticles);
    _isActive = true;
    _emitterMode = Mode::GRAVITY;
    _isAutoRemoveOnFinish = false;
    _transformSystemDirty = false;

    return true;
}

ParticleSystem::~ParticleSystem()
{
}

void ParticleSystem::addParticles(int count)
{
    if (_paused)
    {
        return;
    }
    uint32_t RANDSEED = rand();

    int start = _particleCount;
    _particleCount += count;

    //life
    for (int i = start; i < _particleCount; ++i)
    {
        float theLife = _life + _lifeVar * RANDOM_M11(&RANDSEED);
        _particleData[i].timeToLive = (std::max)(0.0f, theLife);
    }

    //position
    for (int i = start; i < _particleCount; ++i)
    {
        _particleData[i].posx = _sourcePosition.x + _posVar.x * RANDOM_M11(&RANDSEED);
    }

    for (int i = start; i < _particleCount; ++i)
    {
        _particleData[i].posy = _sourcePosition.y + _posVar.y * RANDOM_M11(&RANDSEED);
    }

    //color
#define SET_COLOR(c, b, v)                                                \
    for (int i = start; i < _particleCount; ++i)                          \
    {                                                                     \
        _particleData[i].c = clampf(b + v * RANDOM_M11(&RANDSEED), 0, 1); \
    }

    SET_COLOR(colorR, _startColor.r, _startColorVar.r);
    SET_COLOR(colorG, _startColor.g, _startColorVar.g);
    SET_COLOR(colorB, _startColor.b, _startColorVar.b);
    SET_COLOR(colorA, _startColor.a, _startColorVar.a);

    SET_COLOR(deltaColorR, _endColor.r, _endColorVar.r);
    SET_COLOR(deltaColorG, _endColor.g, _endColorVar.g);
    SET_COLOR(deltaColorB, _endColor.b, _endColorVar.b);
    SET_COLOR(deltaColorA, _endColor.a, _endColorVar.a);

#define SET_DELTA_COLOR(c, dc)                                                                          \
    for (int i = start; i < _particleCount; ++i)                                                        \
    {                                                                                                   \
        _particleData[i].dc = (_particleData[i].dc - _particleData[i].c) / _particleData[i].timeToLive; \
    }

    SET_DELTA_COLOR(colorR, deltaColorR);
    SET_DELTA_COLOR(colorG, deltaColorG);
    SET_DELTA_COLOR(colorB, deltaColorB);
    SET_DELTA_COLOR(colorA, deltaColorA);

    //size
    for (int i = start; i < _particleCount; ++i)
    {
        _particleData[i].size = _startSize + _startSizeVar * RANDOM_M11(&RANDSEED);
        _particleData[i].size = (std::max)(0.0f, _particleData[i].size);
    }

    if (_endSize != START_SIZE_EQUAL_TO_END_SIZE)
    {
        for (int i = start; i < _particleCount; ++i)
        {
            float endSize = _endSize + _endSizeVar * RANDOM_M11(&RANDSEED);
            endSize = (std::max)(0.0f, endSize);
            _particleData[i].deltaSize = (endSize - _particleData[i].size) / _particleData[i].timeToLive;
        }
    }
    else
    {
        for (int i = start; i < _particleCount; ++i)
        {
            _particleData[i].deltaSize = 0.0f;
        }
    }

    // rotation
    for (int i = start; i < _particleCount; ++i)
    {
        _particleData[i].rotation = _startSpin + _startSpinVar * RANDOM_M11(&RANDSEED);
    }
    for (int i = start; i < _particleCount; ++i)
    {
        float endA = _endSpin + _endSpinVar * RANDOM_M11(&RANDSEED);
        _particleData[i].deltaRotation = (endA - _particleData[i].rotation) / _particleData[i].timeToLive;
    }

    // position
    Vec2 pos;
    pos.x = x_;
    pos.y = y_;

    for (int i = start; i < _particleCount; ++i)
    {
        _particleData[i].startPosX = pos.x;
    }
    for (int i = start; i < _particleCount; ++i)
    {
        _particleData[i].startPosY = pos.y;
    }

    // Mode Gravity: A
    if (_emitterMode == Mode::GRAVITY)
    {

        // radial accel
        for (int i = start; i < _particleCount; ++i)
        {
            _particleData[i].modeA.radialAccel = modeA.radialAccel + modeA.radialAccelVar * RANDOM_M11(&RANDSEED);
        }

        // tangential accel
        for (int i = start; i < _particleCount; ++i)
        {
            _particleData[i].modeA.tangentialAccel = modeA.tangentialAccel + modeA.tangentialAccelVar * RANDOM_M11(&RANDSEED);
        }

        // rotation is dir
        if (modeA.rotationIsDir)
        {
            for (int i = start; i < _particleCount; ++i)
            {
                float a = Deg2Rad(_angle + _angleVar * RANDOM_M11(&RANDSEED));
                Vec2 v(cosf(a), sinf(a));
                float s = modeA.speed + modeA.speedVar * RANDOM_M11(&RANDSEED);
                Vec2 dir = v * s;
                _particleData[i].modeA.dirX = dir.x;    //v * s ;
                _particleData[i].modeA.dirY = dir.y;
                _particleData[i].rotation = -Rad2Deg(dir.getAngle());
            }
        }
        else
        {
            for (int i = start; i < _particleCount; ++i)
            {
                float a = Deg2Rad(_angle + _angleVar * RANDOM_M11(&RANDSEED));
                Vec2 v(cosf(a), sinf(a));
                float s = modeA.speed + modeA.speedVar * RANDOM_M11(&RANDSEED);
                Vec2 dir = v * s;
                _particleData[i].modeA.dirX = dir.x;    //v * s ;
                _particleData[i].modeA.dirY = dir.y;
            }
        }
    }

    // Mode Radius: B
    else
    {
        for (int i = start; i < _particleCount; ++i)
        {
            _particleData[i].modeB.radius = modeB.startRadius + modeB.startRadiusVar * RANDOM_M11(&RANDSEED);
        }

        for (int i = start; i < _particleCount; ++i)
        {
            _particleData[i].modeB.angle = Deg2Rad(_angle + _angleVar * RANDOM_M11(&RANDSEED));
        }

        for (int i = start; i < _particleCount; ++i)
        {
            _particleData[i].modeB.degreesPerSecond = Deg2Rad(modeB.rotatePerSecond + modeB.rotatePerSecondVar * RANDOM_M11(&RANDSEED));
        }

        if (modeB.endRadius == START_RADIUS_EQUAL_TO_END_RADIUS)
        {
            for (int i = start; i < _particleCount; ++i)
            {
                _particleData[i].modeB.deltaRadius = 0.0f;
            }
        }
        else
        {
            for (int i = start; i < _particleCount; ++i)
            {
                float endRadius = modeB.endRadius + modeB.endRadiusVar * RANDOM_M11(&RANDSEED);
                _particleData[i].modeB.deltaRadius = (endRadius - _particleData[i].modeB.radius) / _particleData[i].timeToLive;
            }
        }
    }
}

void ParticleSystem::onEntrance()
{
}

void ParticleSystem::onExit()
{
}

void ParticleSystem::stopSystem()
{
    _isActive = false;
    _elapsed = _duration;
    _emitCounter = 0;
}

void ParticleSystem::resetSystem()
{
    _isActive = true;
    _elapsed = 0;
    for (int i = 0; i < _particleCount; ++i)
    {
        _particleData[i].timeToLive = 0.0f;
    }
}

bool ParticleSystem::isFull()
{
    return (_particleCount == _totalParticles);
}

// ParticleSystem - MainLoop
void ParticleSystem::update()
{
    float dt = 1.0 / 25;
    if (_isActive && _emissionRate)
    {
        float rate = 1.0f / _emissionRate;
        int totalParticles = _totalParticles;

        //issue #1201, prevent bursts of particles, due to too high emitCounter
        if (_particleCount < totalParticles)
        {
            _emitCounter += dt;
            if (_emitCounter < 0.f)
            {
                _emitCounter = 0.f;
            }
        }

        int emitCount = (std::min)(1.0f * (totalParticles - _particleCount), _emitCounter / rate);
        addParticles(emitCount);
        _emitCounter -= rate * emitCount;

        _elapsed += dt;
        if (_elapsed < 0.f)
        {
            _elapsed = 0.f;
        }
        if (_duration != DURATION_INFINITY && _duration < _elapsed)
        {
            this->stopSystem();
        }
    }

    for (int i = 0; i < _particleCount; ++i)
    {
        _particleData[i].timeToLive -= dt;
    }

    //ÔÙÉú
    for (int i = 0; i < _particleCount; ++i)
    {
        if (_particleData[i].timeToLive <= 0.0f)
        {
            int j = _particleCount - 1;
            //while (j > 0 && _particleData[i].timeToLive <= 0)
            //{
            //    _particleCount--;
            //    j--;
            //}
            _particleData[i] = _particleData[_particleCount - 1];
            --_particleCount;
        }
    }

    if (_emitterMode == Mode::GRAVITY)
    {
        for (int i = 0; i < _particleCount; ++i)
        {
            Pointf tmp, radial = { 0.0f, 0.0f }, tangential;

            // radial acceleration
            if (_particleData[i].posx || _particleData[i].posy)
            {
                normalize_point(_particleData[i].posx, _particleData[i].posy, &radial);
            }
            tangential = radial;
            radial.x *= _particleData[i].modeA.radialAccel;
            radial.y *= _particleData[i].modeA.radialAccel;

            // tangential acceleration
            std::swap(tangential.x, tangential.y);
            tangential.x *= -_particleData[i].modeA.tangentialAccel;
            tangential.y *= _particleData[i].modeA.tangentialAccel;

            // (gravity + radial + tangential) * dt
            tmp.x = radial.x + tangential.x + modeA.gravity.x;
            tmp.y = radial.y + tangential.y + modeA.gravity.y;
            tmp.x *= dt;
            tmp.y *= dt;

            _particleData[i].modeA.dirX += tmp.x;
            _particleData[i].modeA.dirY += tmp.y;

            // this is cocos2d-x v3.0
            // if (_configName.length()>0 && _yCoordFlipped != -1)

            // this is cocos2d-x v3.0
            tmp.x = _particleData[i].modeA.dirX * dt * _yCoordFlipped;
            tmp.y = _particleData[i].modeA.dirY * dt * _yCoordFlipped;
            _particleData[i].posx += tmp.x;
            _particleData[i].posy -= tmp.y;
        }
    }
    else
    {
        for (int i = 0; i < _particleCount; ++i)
        {
            _particleData[i].modeB.angle += _particleData[i].modeB.degreesPerSecond * dt;
            _particleData[i].modeB.radius += _particleData[i].modeB.deltaRadius * dt;
            _particleData[i].posx = -cosf(_particleData[i].modeB.angle) * _particleData[i].modeB.radius;
            _particleData[i].posy = -sinf(_particleData[i].modeB.angle) * _particleData[i].modeB.radius * _yCoordFlipped;
        }
    }

    //color, size, rotation
    for (int i = 0; i < _particleCount; ++i)
    {
        _particleData[i].colorR += _particleData[i].deltaColorR * dt;
        _particleData[i].colorG += _particleData[i].deltaColorG * dt;
        _particleData[i].colorB += _particleData[i].deltaColorB * dt;
        _particleData[i].colorA += _particleData[i].deltaColorA * dt;
        _particleData[i].size += (_particleData[i].deltaSize * dt);
        _particleData[i].size = (std::max)(0.0f, _particleData[i].size);
        _particleData[i].rotation += _particleData[i].deltaRotation * dt;
    }
}

// ParticleSystem - Texture protocol
void ParticleSystem::setTexture(Texture* var)
{
    if (_texture != var)
    {
        _texture = var;
    }
}

void ParticleSystem::draw()
{
    for (int i = 0; i < _particleCount; i++)
    {
        auto& p = _particleData[i];
        BP_Rect r = { int(p.posx + p.startPosX - _texture->w / 2), int(p.posy + p.startPosY - _texture->h / 2), int(p.size), int(p.size) };
        BP_Color c = { Uint8(p.colorR * 255), Uint8(p.colorG * 255), Uint8(p.colorB * 255), Uint8(p.colorA * 255) };
        TextureManager::getInstance()->renderTexture(_texture, r, c, Uint8(p.colorA * 255));
    }
    update();
}

Texture* ParticleSystem::getTexture() const
{
    return _texture;
}

// ParticleSystem - Properties of Gravity Mode
void ParticleSystem::setTangentialAccel(float t)
{
    modeA.tangentialAccel = t;
}

float ParticleSystem::getTangentialAccel() const
{
    return modeA.tangentialAccel;
}

void ParticleSystem::setTangentialAccelVar(float t)
{
    modeA.tangentialAccelVar = t;
}

float ParticleSystem::getTangentialAccelVar() const
{
    return modeA.tangentialAccelVar;
}

void ParticleSystem::setRadialAccel(float t)
{
    modeA.radialAccel = t;
}

float ParticleSystem::getRadialAccel() const
{
    return modeA.radialAccel;
}

void ParticleSystem::setRadialAccelVar(float t)
{
    modeA.radialAccelVar = t;
}

float ParticleSystem::getRadialAccelVar() const
{
    return modeA.radialAccelVar;
}

void ParticleSystem::setRotationIsDir(bool t)
{
    modeA.rotationIsDir = t;
}

bool ParticleSystem::getRotationIsDir() const
{
    return modeA.rotationIsDir;
}

void ParticleSystem::setGravity(const Vec2& g)
{
    modeA.gravity = g;
}

const Vec2& ParticleSystem::getGravity()
{
    return modeA.gravity;
}

void ParticleSystem::setSpeed(float speed)
{
    modeA.speed = speed;
}

float ParticleSystem::getSpeed() const
{
    return modeA.speed;
}

void ParticleSystem::setSpeedVar(float speedVar)
{

    modeA.speedVar = speedVar;
}

float ParticleSystem::getSpeedVar() const
{

    return modeA.speedVar;
}

// ParticleSystem - Properties of Radius Mode
void ParticleSystem::setStartRadius(float startRadius)
{
    modeB.startRadius = startRadius;
}

float ParticleSystem::getStartRadius() const
{
    return modeB.startRadius;
}

void ParticleSystem::setStartRadiusVar(float startRadiusVar)
{
    modeB.startRadiusVar = startRadiusVar;
}

float ParticleSystem::getStartRadiusVar() const
{
    return modeB.startRadiusVar;
}

void ParticleSystem::setEndRadius(float endRadius)
{
    modeB.endRadius = endRadius;
}

float ParticleSystem::getEndRadius() const
{
    return modeB.endRadius;
}

void ParticleSystem::setEndRadiusVar(float endRadiusVar)
{
    modeB.endRadiusVar = endRadiusVar;
}

float ParticleSystem::getEndRadiusVar() const
{

    return modeB.endRadiusVar;
}

void ParticleSystem::setRotatePerSecond(float degrees)
{
    modeB.rotatePerSecond = degrees;
}

float ParticleSystem::getRotatePerSecond() const
{
    return modeB.rotatePerSecond;
}

void ParticleSystem::setRotatePerSecondVar(float degrees)
{
    modeB.rotatePerSecondVar = degrees;
}

float ParticleSystem::getRotatePerSecondVar() const
{
    return modeB.rotatePerSecondVar;
}

bool ParticleSystem::isActive() const
{
    return _isActive;
}

int ParticleSystem::getTotalParticles() const
{
    return _totalParticles;
}

void ParticleSystem::setTotalParticles(int var)
{
    _totalParticles = var;
}

bool ParticleSystem::isAutoRemoveOnFinish() const
{
    return _isAutoRemoveOnFinish;
}

void ParticleSystem::setAutoRemoveOnFinish(bool var)
{
    _isAutoRemoveOnFinish = var;
}

////don't use a transform matrix, this is faster
//void ParticleSystem::setScale(float s)
//{
//    _transformSystemDirty = true;
//    Node::setScale(s);
//}
//
//void ParticleSystem::setRotation(float newRotation)
//{
//    _transformSystemDirty = true;
//    Node::setRotation(newRotation);
//}
//
//void ParticleSystem::setScaleX(float newScaleX)
//{
//    _transformSystemDirty = true;
//    Node::setScaleX(newScaleX);
//}
//
//void ParticleSystem::setScaleY(float newScaleY)
//{
//    _transformSystemDirty = true;
//    Node::setScaleY(newScaleY);
//}

bool ParticleSystem::isPaused() const
{
    return _paused;
}

void ParticleSystem::pauseEmissions()
{
    _paused = true;
}

void ParticleSystem::resumeEmissions()
{
    _paused = false;
}
