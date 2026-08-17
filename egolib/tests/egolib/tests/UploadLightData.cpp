/// @file UploadLightData.cpp
/// @brief Characterization tests for Upload::upload_light_data (egolib/game/game_wawalite.c).
///
/// WHAT THIS FILE IS
///   Pins the module-lighting upload that feeds the file-scope light_a/light_d/light_nrm
///   globals graphic_lighting_dynalist.c's get_ambient_level and sum_global_lighting read every
///   frame. Both cases below take the "vector is already non-zero" path
///   (game_wawalite.c:35-68); Upload::upload_light_data's zero-vector `else` arm
///   (game_wawalite.c:69-72) calls EngineContext::get().logTarget(), which needs an installed
///   log target, so it is out of scope here (WawaliteReadContractFixture already stands up the
///   heavier bootstrap that arm would need).
///
/// WHY NO FIXTURE
///   Upload::upload_light_data reads only its `data` argument plus the file-scope `gfx.usefaredge`
///   global (game_wawalite.c:40), and writes only the file-scope light_a/light_d/light_nrm
///   globals (lighting.c:28-30). No VFS, GL, SDL or EngineContext service is touched on this
///   path. Bare TEST(), same shape as LightingCache.cpp.
///
/// THE FIX THIS FILE PINS (outdoor/faredge branch only)
///   Upstream commit c5c1bdcf7 ("Lighting: Improve lighting engine", 2015-12-26) rewrote the
///   usefaredge branch from the pre-2015 semantics (recoverable at commit 61eb8c885,
///   game/game.c:4473-4501 -- `light_d = 1.0f; light_a = CLIP(light_a / fTmp, 0, 1);`) to
///   `light_d = light_a * length; light_a = 0.0f;`, WITHOUT updating the consumer
///   (get_ambient_level / sum_global_lighting in graphic_lighting_dynalist.c:48-59,95-98), which
///   still expects glob_amb = light_a*255 and the sunlight peak at light_d*255. With light_a
///   pinned to 0.0f outdoors, every Far Edge (outdoor) module rendered visibly darker than the
///   2.9.0 RC1 reference. This pass restores the pre-2015 semantics. The indoor (non-faredge)
///   branch is untouched by that commit and by this fix; it is pinned unchanged below as a
///   regression guard, not as evidence of a second bug.
///
///   MUTATION CHECK (documented, not asserted): reverting game_wawalite.c's faredge arm to
///   `light_d = light_a * length; light_a = 0.0f;` makes
///   OutdoorFaredgeBranch.RestoresPre2015SunlightSemantics fail -- light_d would read 0.3f
///   (0.2f * 1.5f) instead of 1.0f, and light_a would read 0.0f instead of ~0.13333f.
///
/// NOTE ON VERIFICATION SCOPE: this file characterizes the numeric contract of
/// Upload::upload_light_data in isolation. It does NOT verify the on-screen appearance of any
/// module; that comparison against the 2.9.0 RC1 reference binary is a manual, visual step for a
/// human to perform separately.

#include "gtest/gtest.h"

#include "egolib/game/game.h"
#include "egolib/game/graphic.h"
#include "egolib/game/lighting.h"

namespace
{

/// Scope guard for the module globals this function reads/writes, so one test's fixture values
/// cannot leak into another (gtest_discover_tests gives each test its own process today, but the
/// discipline should not depend on that).
struct LightGlobalsGuard
{
    LightGlobalsGuard()
        : a(light_a), d(light_d), nrm(light_nrm), faredge(gfx.usefaredge)
    {}
    ~LightGlobalsGuard()
    {
        light_a = a;
        light_d = d;
        light_nrm = nrm;
        gfx.usefaredge = faredge;
    }
    LightGlobalsGuard(const LightGlobalsGuard&) = delete;
    LightGlobalsGuard& operator=(const LightGlobalsGuard&) = delete;

    float a;
    float d;
    Ego::Vector3f nrm;
    bool faredge;
};

wawalite_data_t makeWawaliteData(const Ego::Vector3f& lightD, float lightA)
{
    wawalite_data_t data = wawalite_data_t::getDefaults();
    data.light.light_d = lightD;
    data.light.light_a = lightA;
    return data;
}

} // namespace

//============================================================================
// Outdoor (Far Edge) branch -- the one this pass fixes.
//============================================================================

TEST(OutdoorFaredgeBranch, RestoresPre2015SunlightSemantics)
{
    LightGlobalsGuard guard;
    gfx.usefaredge = true;

    // rogue.mod's own wawalite.txt values: light_d (1, 1, 0.5), light_a 0.2. length = 1.5.
    const wawalite_data_t data = makeWawaliteData(Ego::Vector3f(1.0f, 1.0f, 0.5f), 0.2f);
    Upload::upload_light_data(data);

    // Full-strength directional light outdoors.
    EXPECT_FLOAT_EQ(light_d, 1.0f);

    // Ambient carries the length-normalized residual: 0.2 / 1.5.
    EXPECT_NEAR(light_a, 0.2f / 1.5f, 1.0e-6f);

    // light_nrm is normalized to unit length regardless of branch: (1,1,0.5)/1.5.
    EXPECT_NEAR(light_nrm[kX], 2.0f / 3.0f, 1.0e-6f);
    EXPECT_NEAR(light_nrm[kY], 2.0f / 3.0f, 1.0e-6f);
    EXPECT_NEAR(light_nrm[kZ], 1.0f / 3.0f, 1.0e-6f);
}

TEST(OutdoorFaredgeBranch, AmbientResidualIsClampedToOneNotJustNormalized)
{
    // Ego::Math::constrain clamps to [0, 1] (game_wawalite.c:57); light_a / length can exceed 1
    // when the configured ambient is large relative to a short direction vector.
    LightGlobalsGuard guard;
    gfx.usefaredge = true;

    // length = sqrt(0.09) = 0.3; light_a / length = 2.0 / 0.3 = 6.67, clamped to 1.0.
    const wawalite_data_t data = makeWawaliteData(Ego::Vector3f(0.3f, 0.0f, 0.0f), 2.0f);
    Upload::upload_light_data(data);

    EXPECT_FLOAT_EQ(light_d, 1.0f);
    EXPECT_FLOAT_EQ(light_a, 1.0f);
}

//============================================================================
// Indoor (non-faredge) branch -- untouched by this pass; pinned as a regression guard.
//============================================================================

TEST(IndoorBranch, TakesLengthAtFaceValueClampedAndLeavesAmbientUntouched)
{
    LightGlobalsGuard guard;
    gfx.usefaredge = false;

    // length = sqrt(0.09 + 0.16) = 0.5, within [0, 1] so the clamp is a no-op here.
    const wawalite_data_t data = makeWawaliteData(Ego::Vector3f(0.3f, 0.4f, 0.0f), 0.5f);
    Upload::upload_light_data(data);

    EXPECT_FLOAT_EQ(light_d, 0.5f);
    // Indoors, light_a is passed through verbatim from data.light.light_a -- the branch never
    // assigns it (game_wawalite.c:59-65).
    EXPECT_FLOAT_EQ(light_a, 0.5f);

    EXPECT_NEAR(light_nrm[kX], 0.6f, 1.0e-6f);
    EXPECT_NEAR(light_nrm[kY], 0.8f, 1.0e-6f);
    EXPECT_NEAR(light_nrm[kZ], 0.0f, 1.0e-6f);
}

TEST(IndoorBranch, LengthAboveOneIsClampedToOne)
{
    // Same fixture the outdoor test above uses, run through the indoor branch instead: length =
    // 1.5 clamps to 1.0 (game_wawalite.c:64), unlike the outdoor branch's light_d = 1.0f always.
    LightGlobalsGuard guard;
    gfx.usefaredge = false;

    const wawalite_data_t data = makeWawaliteData(Ego::Vector3f(1.0f, 1.0f, 0.5f), 0.2f);
    Upload::upload_light_data(data);

    EXPECT_FLOAT_EQ(light_d, 1.0f);
    EXPECT_FLOAT_EQ(light_a, 0.2f);   // unmodified, unlike the outdoor branch's 0.2/1.5
}
