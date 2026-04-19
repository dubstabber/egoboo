#pragma once

#include "egolib/game/egoboo.h"

#include <memory>
#include <vector>

class Object;
namespace Ego
{
class Particle;
class Texture;
}

class IParticleHandler
{
public:
    using ParticleList = std::vector<std::shared_ptr<Ego::Particle>>;

    class ParticleIterator
    {
    public:
        using const_iterator = ParticleList::const_iterator;

        const_iterator begin() const
        {
            return _handler.beginActiveParticles();
        }

        const_iterator end() const
        {
            return _handler.endActiveParticles();
        }

        const_iterator cbegin() const
        {
            return _handler.beginActiveParticles();
        }

        const_iterator cend() const
        {
            return _handler.endActiveParticles();
        }

        ~ParticleIterator()
        {
            _handler.unlockParticles();
        }

        ParticleIterator(const ParticleIterator& other) :
            _handler(other._handler)
        {
            _handler.lockParticles();
        }

        ParticleIterator& operator=(const ParticleIterator&) = delete;

    private:
        explicit ParticleIterator(IParticleHandler& handler) :
            _handler(handler)
        {
            _handler.lockParticles();
        }

        IParticleHandler& _handler;

        friend class IParticleHandler;
    };

    virtual ~IParticleHandler() = default;

    ParticleIterator iterator()
    {
        return ParticleIterator(*this);
    }

    ParticleIterator iterator() const
    {
        return ParticleIterator(const_cast<IParticleHandler&>(*this));
    }

    virtual void updateAllParticles() = 0;
    virtual void download(egoboo_config_t& cfg) = 0;
    virtual void upload(egoboo_config_t& cfg) = 0;
    virtual size_t getDisplayLimit() const = 0;
    virtual void setDisplayLimit(size_t displayLimit) = 0;
    virtual void clear() = 0;
    virtual const std::shared_ptr<Ego::Particle>& operator[](ParticleRef index) = 0;
    virtual std::shared_ptr<Ego::Particle> spawnLocalParticle(const Ego::Vector3f& position,
                                                              const Facing& facing,
                                                              ObjectProfileRef profile,
                                                              const LocalParticleProfileRef& particleProfile,
                                                              ObjectRef attachment,
                                                              uint16_t vertexOffset,
                                                              TEAM_REF team,
                                                              ObjectRef origin,
                                                              ParticleRef particleOrigin,
                                                              int multispawn,
                                                              ObjectRef target) = 0;
    virtual std::shared_ptr<Ego::Particle> spawnParticle(const Ego::Vector3f& spawnPos,
                                                         const Facing& spawnFacing,
                                                         ObjectProfileRef spawnProfile,
                                                         PIP_REF particleProfile,
                                                         ObjectRef spawnAttach,
                                                         uint16_t vertexOffset,
                                                         TEAM_REF spawnTeam,
                                                         ObjectRef spawnOrigin,
                                                         ParticleRef spawnParticleOrigin = ParticleRef::Invalid,
                                                         int multispawn = 0,
                                                         ObjectRef spawnTarget = ObjectRef::Invalid,
                                                         bool onlyOverWater = false) = 0;
    virtual std::shared_ptr<Ego::Particle> spawnGlobalParticle(const Ego::Vector3f& spawnPos,
                                                               const Facing& spawnFacing,
                                                               const LocalParticleProfileRef& particleProfile,
                                                               int multispawn,
                                                               bool onlyOverWater = false) = 0;
    virtual size_t getCount() const = 0;
    virtual size_t getFreeCount() const = 0;
    virtual std::shared_ptr<const Ego::Texture> getLightParticleTexture() = 0;
    virtual std::shared_ptr<const Ego::Texture> getTransparentParticleTexture() = 0;
    virtual void spawnPoof(const std::shared_ptr<Object>& object) = 0;
    virtual void spawnDefencePing(const std::shared_ptr<Object>& object, const std::shared_ptr<Object>& attacker) = 0;

protected:
    virtual ParticleList::const_iterator beginActiveParticles() = 0;
    virtual ParticleList::const_iterator endActiveParticles() = 0;
    virtual void lockParticles() = 0;
    virtual void unlockParticles() = 0;
};
