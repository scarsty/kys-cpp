#pragma once

//移植自Cocos2dx，版权声明请查看licenses文件夹

#include "Element.h"
#include "Point.h"
#include "TextureManager.h"
#include <vector>

struct Pointf
{
public:
    Pointf() {}
    Pointf(float _x, float _y)
        : x(_x)
        , y(_y)
    {
    }
    ~Pointf() {}
    float x = 0, y = 0;
    Pointf operator*(float f)
    {
        Pointf p{ x * f, y * f };
        return p;
    }
    float getAngle()
    {
        return atan2f(y, x);
    }
};

typedef Pointf Vec2;

//class ParticleBatchNode;

struct Color4F
{
    float r, g, b, a;
};

class ParticleData
{
public:
    float posx;
    float posy;
    float startPosX;
    float startPosY;

    float colorR;
    float colorG;
    float colorB;
    float colorA;

    float deltaColorR;
    float deltaColorG;
    float deltaColorB;
    float deltaColorA;

    float size;
    float deltaSize;
    float rotation;
    float deltaRotation;
    float timeToLive;
    unsigned int atlasIndex;

    //! Mode A: gravity, direction, radial accel, tangential accel
    struct
    {
        float dirX;
        float dirY;
        float radialAccel;
        float tangentialAccel;
    } modeA;

    //! Mode B: radius mode
    struct
    {
        float angle;
        float degreesPerSecond;
        float radius;
        float deltaRadius;
    } modeB;
};

//typedef void (*CC_UPDATE_PARTICLE_IMP)(id, SEL, tParticle*, Vec2);

/** @class ParticleSystem
 * @brief Particle System base class.
Attributes of a Particle System:
- emission rate of the particles
- Gravity Mode (Mode A):
- gravity
- direction
- speed +-  variance
- tangential acceleration +- variance
- radial acceleration +- variance
- Radius Mode (Mode B):
- startRadius +- variance
- endRadius +- variance
- rotate +- variance
- Properties common to all modes:
- life +- life variance
- start spin +- variance
- end spin +- variance
- start size +- variance
- end size +- variance
- start color +- variance
- end color +- variance
- life +- variance
- blending function
- texture

@code
emitter.radialAccel = 15;
emitter.startSpin = 0;
@endcode

*/

class ParticleSystem : public Element
{
public:
    enum class Mode
    {
        GRAVITY,
        RADIUS,
    };

    enum
    {
        /** The Particle emitter lives forever. */
        DURATION_INFINITY = -1,

        /** The starting size of the particle is equal to the ending size. */
        START_SIZE_EQUAL_TO_END_SIZE = -1,

        /** The starting radius of the particle is equal to the ending radius. */
        START_RADIUS_EQUAL_TO_END_RADIUS = -1,
    };

public:
    void addParticles(int count);

    void stopSystem();
    /** Kill all living particles.
     */
    void resetSystem();
    /** Whether or not the system is full.
     *
     * @return True if the system is full.
     */
    bool isFull();

    /** Whether or not the particle system removed self on finish.
     *
     * @return True if the particle system removed self on finish.
     */
    virtual bool isAutoRemoveOnFinish() const;

    /** Set the particle system auto removed it self on finish.
     *
     * @param var True if the particle system removed self on finish.
     */
    virtual void setAutoRemoveOnFinish(bool var);

    // mode A
    /** Gets the gravity.
     *
     * @return The gravity.
     */
    virtual const Vec2& getGravity();
    /** Sets the gravity.
     *
     * @param g The gravity.
     */
    virtual void setGravity(const Vec2& g);
    /** Gets the speed.
     *
     * @return The speed.
     */
    virtual float getSpeed() const;
    /** Sets the speed.
     *
     * @param speed The speed.
     */
    virtual void setSpeed(float speed);
    /** Gets the speed variance.
     *
     * @return The speed variance.
     */
    virtual float getSpeedVar() const;
    /** Sets the speed variance.
     *
     * @param speed The speed variance.
     */
    virtual void setSpeedVar(float speed);
    /** Gets the tangential acceleration.
     *
     * @return The tangential acceleration.
     */
    virtual float getTangentialAccel() const;
    /** Sets the tangential acceleration.
     *
     * @param t The tangential acceleration.
     */
    virtual void setTangentialAccel(float t);
    /** Gets the tangential acceleration variance.
     *
     * @return The tangential acceleration variance.
     */
    virtual float getTangentialAccelVar() const;
    /** Sets the tangential acceleration variance.
     *
     * @param t The tangential acceleration variance.
     */
    virtual void setTangentialAccelVar(float t);
    /** Gets the radial acceleration.
     *
     * @return The radial acceleration.
     */
    virtual float getRadialAccel() const;
    /** Sets the radial acceleration.
     *
     * @param t The radial acceleration.
     */
    virtual void setRadialAccel(float t);
    /** Gets the radial acceleration variance.
     *
     * @return The radial acceleration variance.
     */
    virtual float getRadialAccelVar() const;
    /** Sets the radial acceleration variance.
     *
     * @param t The radial acceleration variance.
     */
    virtual void setRadialAccelVar(float t);
    /** Whether or not the rotation of each particle to its direction.
     *
     * @return True if the rotation is the direction.
     */
    virtual bool getRotationIsDir() const;
    /** Sets the rotation of each particle to its direction.
     *
     * @param t True if the rotation is the direction.
     */
    virtual void setRotationIsDir(bool t);
    // mode B
    /** Gets the start radius.
     *
     * @return The start radius.
     */
    virtual float getStartRadius() const;
    /** Sets the start radius.
     *
     * @param startRadius The start radius.
     */
    virtual void setStartRadius(float startRadius);
    /** Gets the start radius variance.
     *
     * @return The start radius variance.
     */
    virtual float getStartRadiusVar() const;
    /** Sets the start radius variance.
     *
     * @param startRadiusVar The start radius variance.
     */
    virtual void setStartRadiusVar(float startRadiusVar);
    /** Gets the end radius.
     *
     * @return The end radius.
     */
    virtual float getEndRadius() const;
    /** Sets the end radius.
     *
     * @param endRadius The end radius.
     */
    virtual void setEndRadius(float endRadius);
    /** Gets the end radius variance.
     *
     * @return The end radius variance.
     */
    virtual float getEndRadiusVar() const;
    /** Sets the end radius variance.
     *
     * @param endRadiusVar The end radius variance.
     */
    virtual void setEndRadiusVar(float endRadiusVar);
    /** Gets the number of degrees to rotate a particle around the source pos per second.
     *
     * @return The number of degrees to rotate a particle around the source pos per second.
     */
    virtual float getRotatePerSecond() const;
    /** Sets the number of degrees to rotate a particle around the source pos per second.
     *
     * @param degrees The number of degrees to rotate a particle around the source pos per second.
     */
    virtual void setRotatePerSecond(float degrees);
    /** Gets the rotate per second variance.
     *
     * @return The rotate per second variance.
     */
    virtual float getRotatePerSecondVar() const;
    /** Sets the rotate per second variance.
     *
     * @param degrees The rotate per second variance.
     */
    virtual void setRotatePerSecondVar(float degrees);

    //virtual void setScale(float s);
    //virtual void setRotation(float newRotation);
    //virtual void setScaleX(float newScaleX);
    //virtual void setScaleY(float newScaleY);

    /** Whether or not the particle system is active.
     *
     * @return True if the particle system is active.
     */
    virtual bool isActive() const;
    /** Whether or not the particle system is blend additive.
     *
     * @return True if the particle system is blend additive.
     */
    //virtual bool isBlendAdditive() const;
    /** Sets the particle system blend additive.
     *
     * @param value True if the particle system is blend additive.
     */
    //virtual void setBlendAdditive(bool value);

    /** Gets the batch node.
     *
     * @return The batch node.
     */
    //virtual ParticleBatchNode* getBatchNode() const;
    /** Sets the batch node.
     *
     * @param batchNode The batch node.
     */
    //virtual void setBatchNode(ParticleBatchNode* batchNode);

    /** Gets the index of system in batch node array.
     *
     * @return The index of system in batch node array.
     */
    int getAtlasIndex() const { return _atlasIndex; }
    /** Sets the index of system in batch node array.
     *
     * @param index The index of system in batch node array.
     */
    void setAtlasIndex(int index) { _atlasIndex = index; }

    /** Gets the Quantity of particles that are being simulated at the moment.
     *
     * @return The Quantity of particles that are being simulated at the moment.
     */
    unsigned int getParticleCount() const { return _particleCount; }

    /** Gets how many seconds the emitter will run. -1 means 'forever'.
     *
     * @return The seconds that the emitter will run. -1 means 'forever'.
     */
    float getDuration() const { return _duration; }
    /** Sets how many seconds the emitter will run. -1 means 'forever'.
     *
     * @param duration The seconds that the emitter will run. -1 means 'forever'.
     */
    void setDuration(float duration) { _duration = duration; }

    /** Gets the source position of the emitter.
     *
     * @return The source position of the emitter.
     */
    const Vec2& getSourcePosition() const { return _sourcePosition; }
    /** Sets the source position of the emitter.
     *
     * @param pos The source position of the emitter.
     */
    void setSourcePosition(const Vec2& pos) { _sourcePosition = pos; }

    /** Gets the position variance of the emitter.
     *
     * @return The position variance of the emitter.
     */
    const Vec2& getPosVar() const { return _posVar; }
    /** Sets the position variance of the emitter.
     *
     * @param pos The position variance of the emitter.
     */
    void setPosVar(const Vec2& pos) { _posVar = pos; }

    /** Gets the life of each particle.
     *
     * @return The life of each particle.
     */
    float getLife() const { return _life; }
    /** Sets the life of each particle.
     *
     * @param life The life of each particle.
     */
    void setLife(float life) { _life = life; }

    /** Gets the life variance of each particle.
     *
     * @return The life variance of each particle.
     */
    float getLifeVar() const { return _lifeVar; }
    /** Sets the life variance of each particle.
     *
     * @param lifeVar The life variance of each particle.
     */
    void setLifeVar(float lifeVar) { _lifeVar = lifeVar; }

    /** Gets the angle of each particle.
     *
     * @return The angle of each particle.
     */
    float getAngle() const { return _angle; }
    /** Sets the angle of each particle.
     *
     * @param angle The angle of each particle.
     */
    void setAngle(float angle) { _angle = angle; }

    /** Gets the angle variance of each particle.
     *
     * @return The angle variance of each particle.
     */
    float getAngleVar() const { return _angleVar; }
    /** Sets the angle variance of each particle.
     *
     * @param angleVar The angle variance of each particle.
     */
    void setAngleVar(float angleVar) { _angleVar = angleVar; }

    /** Switch between different kind of emitter modes:
     - kParticleModeGravity: uses gravity, speed, radial and tangential acceleration.
     - kParticleModeRadius: uses radius movement + rotation.
     *
     * @return The mode of the emitter.
     */
    Mode getEmitterMode() const { return _emitterMode; }
    /** Sets the mode of the emitter.
     *
     * @param mode The mode of the emitter.
     */
    void setEmitterMode(Mode mode) { _emitterMode = mode; }

    /** Gets the start size in pixels of each particle.
     *
     * @return The start size in pixels of each particle.
     */
    float getStartSize() const { return _startSize; }
    /** Sets the start size in pixels of each particle.
     *
     * @param startSize The start size in pixels of each particle.
     */
    void setStartSize(float startSize) { _startSize = startSize; }

    /** Gets the start size variance in pixels of each particle.
     *
     * @return The start size variance in pixels of each particle.
     */
    float getStartSizeVar() const { return _startSizeVar; }
    /** Sets the start size variance in pixels of each particle.
     *
     * @param sizeVar The start size variance in pixels of each particle.
     */
    void setStartSizeVar(float sizeVar) { _startSizeVar = sizeVar; }

    /** Gets the end size in pixels of each particle.
     *
     * @return The end size in pixels of each particle.
     */
    float getEndSize() const { return _endSize; }
    /** Sets the end size in pixels of each particle.
     *
     * @param endSize The end size in pixels of each particle.
     */
    void setEndSize(float endSize) { _endSize = endSize; }

    /** Gets the end size variance in pixels of each particle.
     *
     * @return The end size variance in pixels of each particle.
     */
    float getEndSizeVar() const { return _endSizeVar; }
    /** Sets the end size variance in pixels of each particle.
     *
     * @param sizeVar The end size variance in pixels of each particle.
     */
    void setEndSizeVar(float sizeVar) { _endSizeVar = sizeVar; }

    /** Gets the start color of each particle.
     *
     * @return The start color of each particle.
     */
    const Color4F& getStartColor() const { return _startColor; }
    /** Sets the start color of each particle.
     *
     * @param color The start color of each particle.
     */
    void setStartColor(const Color4F& color) { _startColor = color; }

    /** Gets the start color variance of each particle.
     *
     * @return The start color variance of each particle.
     */
    const Color4F& getStartColorVar() const { return _startColorVar; }
    /** Sets the start color variance of each particle.
     *
     * @param color The start color variance of each particle.
     */
    void setStartColorVar(const Color4F& color) { _startColorVar = color; }

    /** Gets the end color and end color variation of each particle.
     *
     * @return The end color and end color variation of each particle.
     */
    const Color4F& getEndColor() const { return _endColor; }
    /** Sets the end color and end color variation of each particle.
     *
     * @param color The end color and end color variation of each particle.
     */
    void setEndColor(const Color4F& color) { _endColor = color; }

    /** Gets the end color variance of each particle.
     *
     * @return The end color variance of each particle.
     */
    const Color4F& getEndColorVar() const { return _endColorVar; }
    /** Sets the end color variance of each particle.
     *
     * @param color The end color variance of each particle.
     */
    void setEndColorVar(const Color4F& color) { _endColorVar = color; }

    /** Gets the start spin of each particle.
     *
     * @return The start spin of each particle.
     */
    float getStartSpin() const { return _startSpin; }
    /** Sets the start spin of each particle.
     *
     * @param spin The start spin of each particle.
     */
    void setStartSpin(float spin) { _startSpin = spin; }

    /** Gets the start spin variance of each particle.
     *
     * @return The start spin variance of each particle.
     */
    float getStartSpinVar() const { return _startSpinVar; }
    /** Sets the start spin variance of each particle.
     *
     * @param pinVar The start spin variance of each particle.
     */
    void setStartSpinVar(float pinVar) { _startSpinVar = pinVar; }

    /** Gets the end spin of each particle.
     *
     * @return The end spin of each particle.
     */
    float getEndSpin() const { return _endSpin; }
    /** Sets the end spin of each particle.
     *
     * @param endSpin The end spin of each particle.
     */
    void setEndSpin(float endSpin) { _endSpin = endSpin; }

    /** Gets the end spin variance of each particle.
     *
     * @return The end spin variance of each particle.
     */
    float getEndSpinVar() const { return _endSpinVar; }
    /** Sets the end spin variance of each particle.
     *
     * @param endSpinVar The end spin variance of each particle.
     */
    void setEndSpinVar(float endSpinVar) { _endSpinVar = endSpinVar; }

    /** Gets the emission rate of the particles.
     *
     * @return The emission rate of the particles.
     */
    float getEmissionRate() const { return _emissionRate; }
    /** Sets the emission rate of the particles.
     *
     * @param rate The emission rate of the particles.
     */
    void setEmissionRate(float rate) { _emissionRate = rate; }

    /** Gets the maximum particles of the system.
     *
     * @return The maximum particles of the system.
     */
    virtual int getTotalParticles() const;
    /** Sets the maximum particles of the system.
     *
     * @param totalParticles The maximum particles of the system.
     */
    virtual void setTotalParticles(int totalParticles);

    /** does the alpha value modify color */
    void setOpacityModifyRGB(bool opacityModifyRGB) { _opacityModifyRGB = opacityModifyRGB; }
    bool isOpacityModifyRGB() const { return _opacityModifyRGB; }

    /** Gets the particles movement type: Free or Grouped.
     @since v0.8
     *
     * @return The particles movement type.
     */
    //PositionType getPositionType() const { return _positionType; }
    /** Sets the particles movement type: Free or Grouped.
    @since v0.8
     *
     * @param type The particles movement type.
     */
    //void setPositionType(PositionType type) { _positionType = type; }

    // Overrides
    virtual void onEntrance() override;
    virtual void onExit() override;
    virtual Texture* getTexture() const;
    virtual void setTexture(Texture* texture);
    virtual void draw() override;
    void update();

    ParticleSystem();
    virtual ~ParticleSystem();

    /** initializes a ParticleSystem*/
    virtual bool init();
    virtual bool initWithTotalParticles(int numberOfParticles);
    virtual bool isPaused() const;
    virtual void pauseEmissions();
    virtual void resumeEmissions();

protected:
    //virtual void updateBlendFunc();

private:
    friend class EngineDataManager;
    /** Internal use only, it's used by EngineDataManager class for Android platform */
    static void setTotalParticleCountFactor(float factor);

protected:
    /** whether or not the particles are using blend additive.
     If enabled, the following blending function will be used.
     @code
     source blend function = GL_SRC_ALPHA;
     dest blend function = GL_ONE;
     @endcode
     */
    bool _isBlendAdditive;

    /** whether or not the node will be auto-removed when it has no particles left.
     By default it is false.
     @since v0.8
     */
    bool _isAutoRemoveOnFinish;

    std::string _plistFile;
    //! time elapsed since the start of the system (in seconds)
    float _elapsed;

    // Different modes
    //! Mode A:Gravity + Tangential Accel + Radial Accel
    struct
    {
        /** Gravity value. Only available in 'Gravity' mode. */
        Vec2 gravity;
        /** speed of each particle. Only available in 'Gravity' mode.  */
        float speed;
        /** speed variance of each particle. Only available in 'Gravity' mode. */
        float speedVar;
        /** tangential acceleration of each particle. Only available in 'Gravity' mode. */
        float tangentialAccel;
        /** tangential acceleration variance of each particle. Only available in 'Gravity' mode. */
        float tangentialAccelVar;
        /** radial acceleration of each particle. Only available in 'Gravity' mode. */
        float radialAccel;
        /** radial acceleration variance of each particle. Only available in 'Gravity' mode. */
        float radialAccelVar;
        /** set the rotation of each particle to its direction Only available in 'Gravity' mode. */
        bool rotationIsDir;
    } modeA;

    //! Mode B: circular movement (gravity, radial accel and tangential accel don't are not used in this mode)
    struct
    {
        /** The starting radius of the particles. Only available in 'Radius' mode. */
        float startRadius;
        /** The starting radius variance of the particles. Only available in 'Radius' mode. */
        float startRadiusVar;
        /** The ending radius of the particles. Only available in 'Radius' mode. */
        float endRadius;
        /** The ending radius variance of the particles. Only available in 'Radius' mode. */
        float endRadiusVar;
        /** Number of degrees to rotate a particle around the source pos per second. Only available in 'Radius' mode. */
        float rotatePerSecond;
        /** Variance in degrees for rotatePerSecond. Only available in 'Radius' mode. */
        float rotatePerSecondVar;
    } modeB;

    //particle data
    std::vector<ParticleData> _particleData;

    //Emitter name
    std::string _configName;

    // color modulate
    //    BOOL colorModulate;

    //! How many particles can be emitted per second
    float _emitCounter;

    // Optimization
    //CC_UPDATE_PARTICLE_IMP    updateParticleImp;
    //SEL                        updateParticleSel;

    /** weak reference to the SpriteBatchNode that renders the Sprite */
    //ParticleBatchNode* _batchNode;

    // index of system in batch node array
    int _atlasIndex;

    //true if scaled or rotated
    bool _transformSystemDirty;
    // Number of allocated particles
    int _allocatedParticles;

    /** Is the emitter active */
    bool _isActive;

    /** Quantity of particles that are being simulated at the moment */
    int _particleCount;

    /** How many seconds the emitter will run. -1 means 'forever' */
    float _duration;
    /** sourcePosition of the emitter */
    Vec2 _sourcePosition;
    /** Position variance of the emitter */
    Vec2 _posVar;
    /** life, and life variation of each particle */
    float _life;
    /** life variance of each particle */
    float _lifeVar;
    /** angle and angle variation of each particle */
    float _angle;
    /** angle variance of each particle */
    float _angleVar;

    /** Switch between different kind of emitter modes:
     - kParticleModeGravity: uses gravity, speed, radial and tangential acceleration
     - kParticleModeRadius: uses radius movement + rotation
     */
    Mode _emitterMode;

    /** start size in pixels of each particle */
    float _startSize;
    /** size variance in pixels of each particle */
    float _startSizeVar;
    /** end size in pixels of each particle */
    float _endSize;
    /** end size variance in pixels of each particle */
    float _endSizeVar;
    /** start color of each particle */
    Color4F _startColor;
    /** start color variance of each particle */
    Color4F _startColorVar;
    /** end color and end color variation of each particle */
    Color4F _endColor;
    /** end color variance of each particle */
    Color4F _endColorVar;
    //* initial angle of each particle
    float _startSpin;
    //* initial angle of each particle
    float _startSpinVar;
    //* initial angle of each particle
    float _endSpin;
    //* initial angle of each particle
    float _endSpinVar;
    /** emission rate of the particles */
    float _emissionRate;
    /** maximum particles of the system */
    int _totalParticles;
    /** conforms to CocosNodeTexture protocol */
    Texture* _texture;
    /** conforms to CocosNodeTexture protocol */
    //BlendFunc _blendFunc;
    /** does the alpha value modify color */
    bool _opacityModifyRGB;
    /** does FlippedY variance of each particle */
    int _yCoordFlipped;

    /** particles movement type: Free or Grouped
     @since v0.8
     */
    //PositionType _positionType;

    /** is the emitter paused */
    bool _paused;

    /** is sourcePosition compatible */
    bool _sourcePositionCompatible;

    static std::vector<ParticleSystem*> __allInstances;
};
