//********************************************************************************************
//*
//*    This file is part of Egoboo.
//*
//*    Egoboo is free software: you can redistribute it and/or modify it
//*    under the terms of the GNU General Public License as published by
//*    the Free Software Foundation, either version 3 of the License, or
//*    (at your option) any later version.
//*
//*    Egoboo is distributed in the hope that it will be useful, but
//*    WITHOUT ANY WARRANTY; without even the implied warranty of
//*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//*    General Public License for more details.
//*
//*    You should have received a copy of the GNU General Public License
//*    along with Egoboo.  If not, see <http://www.gnu.org/licenses/>.
//*
//********************************************************************************************

/// @file egolib/tests/egolib/tests/PlayingStateTeardown.cpp
/// @brief Pins shouldExportPlayersOnShutdown(), the decision logic extracted from
///        PlayingState::~PlayingState()'s player-export-on-shutdown branch. Extracted (rather
///        than exercised via a constructed PlayingState) because PlayingState's constructor
///        requires a live UIManager/GraphicsWindow/active module -- the same GL/UIManager
///        coupling that blocks headless construction of other GUI-heavy widgets in this suite.

#include "gtest/gtest.h"

#include "egolib/game/GameStates/PlayingState.hpp"
#include "egolib/game/Module/IModuleStatus.hpp"
#include "egolib/Profiles/IProfileSystem.hpp"
#include "egolib/Renderer/DeferredTexture.hpp"

namespace
{

class StubModuleStatus : public IModuleStatus
{
public:
    explicit StubModuleStatus(bool exportValid) : _exportValid(exportValid) {}

    bool isExportValid() const override { return _exportValid; }
    bool isRespawnValid() const override { return false; }
    bool canRespawnAnyTime() const override { return false; }
    bool isBeaten() const override { return false; }
    int passageCount() const override { return 0; }
    const std::shared_ptr<ModuleProfile>& moduleProfile() const override { return _moduleProfile; }
    const std::list<std::string>& importPlayers() const override { return _importPlayers; }

private:
    bool _exportValid;
    std::shared_ptr<ModuleProfile> _moduleProfile;
    std::list<std::string> _importPlayers;
};

class StubProfileSystem : public IProfileSystem
{
public:
    void reset() override {}
    bool isLoaded(PRO_REF) const override { return false; }
    bool isLoaded(ObjectProfileRef) const override { return false; }
    ObjectProfileRef loadOneProfile(const std::string&, int) override { return ObjectProfileRef::Invalid; }
    const std::shared_ptr<ObjectProfile>& getProfile(PRO_REF) const override { return _objectProfile; }
    const std::shared_ptr<ObjectProfile>& getProfile(ObjectProfileRef) const override { return _objectProfile; }
    const std::unordered_map<PRO_REF, std::shared_ptr<ObjectProfile>>& getLoadedProfiles() const override { return _loadedProfiles; }
    const Ego::DeferredTexture& getSpellBookIcon(size_t) const override { return _spellbookIcon; }
    void loadModuleProfiles() override {}
    const std::vector<std::shared_ptr<ModuleProfile>>& getModuleProfiles() const override { return _moduleProfiles; }
    void loadAllSavedCharacters(const std::string&) override {}
    const std::vector<std::shared_ptr<LoadPlayerElement>>& getSavedPlayers() const override { return _savedPlayers; }
    void loadGlobalParticleProfiles() override {}
    bool isParticleProfileLoaded(PIP_REF) const override { return false; }
    const std::shared_ptr<ParticleProfile>& getParticleProfile(PIP_REF) const override { return _particleProfile; }
    PIP_REF loadParticleProfile(const std::string&, PIP_REF) override { return INVALID_PIP_REF; }
    void unloadParticleProfile(PIP_REF) override {}
    bool isEnchantProfileLoaded(EVE_REF) const override { return false; }
    const std::shared_ptr<EnchantProfile>& getEnchantProfile(EVE_REF) const override { return _enchantProfile; }
    EVE_REF loadEnchantProfile(const std::string&, EVE_REF) override { return INVALID_EVE_REF; }

private:
    Ego::DeferredTexture _spellbookIcon;
    std::shared_ptr<ObjectProfile> _objectProfile;
    std::shared_ptr<ParticleProfile> _particleProfile;
    std::shared_ptr<EnchantProfile> _enchantProfile;
    std::unordered_map<PRO_REF, std::shared_ptr<ObjectProfile>> _loadedProfiles;
    std::vector<std::shared_ptr<ModuleProfile>> _moduleProfiles;
    std::vector<std::shared_ptr<LoadPlayerElement>> _savedPlayers;
};

} // namespace

TEST(PlayingStateTeardown, SkipsExportWhenNoModuleStatusInstalled)
{
    StubProfileSystem profileSystem;
    EXPECT_FALSE(shouldExportPlayersOnShutdown(nullptr, &profileSystem));
}

TEST(PlayingStateTeardown, SkipsExportWhenModuleStatusIsNotExportValid)
{
    StubModuleStatus moduleStatus(false);
    StubProfileSystem profileSystem;
    EXPECT_FALSE(shouldExportPlayersOnShutdown(&moduleStatus, &profileSystem));
}

// This is the exact shape of defect (1): on the abnormal EngineContext::clearEngine() teardown
// corridor (an exception escaping the main loop makes Main.cpp call clearEngine() directly,
// bypassing GameEngine::uninitialize()), the profile-system registry is cleared before
// PlayingState's destructor runs even though the export-valid module-status registry (a separate,
// GameSessionContext-owned seam that clearEngine() never touches) is still installed. Before the
// fix, PlayingState::~PlayingState() called the throwing EngineContext::get().profileSystem()
// unconditionally in exactly this state, raising std::logic_error ("no active profile system",
// IProfileSystem.cpp:30) from inside a destructor -- terminating the process.
TEST(PlayingStateTeardown, SkipsExportWhenProfileSystemIsGoneEvenIfExportValid)
{
    StubModuleStatus moduleStatus(true);
    EXPECT_FALSE(shouldExportPlayersOnShutdown(&moduleStatus, nullptr));
}

TEST(PlayingStateTeardown, ExportsWhenModuleStatusIsExportValidAndProfileSystemIsInstalled)
{
    StubModuleStatus moduleStatus(true);
    StubProfileSystem profileSystem;
    EXPECT_TRUE(shouldExportPlayersOnShutdown(&moduleStatus, &profileSystem));
}
