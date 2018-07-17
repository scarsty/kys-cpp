#pragma once
#include "ParticleSystem.h"

/** @class ParticleFire
 * @brief A fire particle system.
 */
class ParticleFire : public ParticleSystem
{
public:
    ParticleFire() {}
    virtual ~ParticleFire() {}

    bool init() override { return initWithTotalParticles(250); }
    virtual bool initWithTotalParticles(int numberOfParticles) override;
};

/** @class ParticleFireworks
 * @brief A fireworks particle system.
 */
class ParticleFireworks : public ParticleSystem
{
public:
    ParticleFireworks() {}
    virtual ~ParticleFireworks() {}

    bool init() { return initWithTotalParticles(1500); }
    virtual bool initWithTotalParticles(int numberOfParticles);
};

/** @class ParticleSun
 * @brief A sun particle system.
 */
class ParticleSun : public ParticleSystem
{
public:
    ParticleSun() {}
    virtual ~ParticleSun() {}

    bool init() { return initWithTotalParticles(350); }
    virtual bool initWithTotalParticles(int numberOfParticles);
};

/** @class ParticleGalaxy
 * @brief A galaxy particle system.
 */
class ParticleGalaxy : public ParticleSystem
{
public:
    ParticleGalaxy() {}
    virtual ~ParticleGalaxy() {}

    bool init() { return initWithTotalParticles(200); }
    virtual bool initWithTotalParticles(int numberOfParticles);
};

/** @class ParticleFlower
 * @brief A flower particle system.
 */
class ParticleFlower : public ParticleSystem
{
public:
    ParticleFlower() {}
    virtual ~ParticleFlower() {}

    bool init() { return initWithTotalParticles(250); }
    virtual bool initWithTotalParticles(int numberOfParticles);
};

/** @class ParticleMeteor
 * @brief A meteor particle system.
 */
class ParticleMeteor : public ParticleSystem
{
public:
    ParticleMeteor() {}
    virtual ~ParticleMeteor() {}

    bool init() { return initWithTotalParticles(150); }
    virtual bool initWithTotalParticles(int numberOfParticles);
};

/** @class ParticleSpiral
 * @brief An spiral particle system.
 */
class ParticleSpiral : public ParticleSystem
{
public:
    ParticleSpiral() {}
    ~ParticleSpiral() {}

    bool init() { return initWithTotalParticles(500); }
    virtual bool initWithTotalParticles(int numberOfParticles);
};

/** @class ParticleExplosion
 * @brief An explosion particle system.
 */
class ParticleExplosion : public ParticleSystem
{
public:
    ParticleExplosion() {}
    virtual ~ParticleExplosion() {}

    bool init() { return initWithTotalParticles(700); }
    virtual bool initWithTotalParticles(int numberOfParticles);
};

/** @class ParticleSmoke
 * @brief An smoke particle system.
 */
class ParticleSmoke : public ParticleSystem
{
public:
    ParticleSmoke() {}
    virtual ~ParticleSmoke() {}

    bool init() { return initWithTotalParticles(200); }
    virtual bool initWithTotalParticles(int numberOfParticles);
};

/** @class ParticleSnow
 * @brief An snow particle system.
 */
class ParticleSnow : public ParticleSystem
{
public:
    ParticleSnow() {}
    virtual ~ParticleSnow() {}

    bool init() { return initWithTotalParticles(700); }
    virtual bool initWithTotalParticles(int numberOfParticles);
};

/** @class ParticleRain
 * @brief A rain particle system.
 */
class ParticleRain : public ParticleSystem
{
public:
    ParticleRain() {}
    virtual ~ParticleRain() {}

    bool init() { return initWithTotalParticles(1000); }
    virtual bool initWithTotalParticles(int numberOfParticles);
};

class ParticleCreator
{
    static ParticleSystem* create(const std::string type);
};
