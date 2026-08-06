#include "gtest/gtest.h"

#include "egolib/game/lighting.h"

#include <cmath>
#include <limits>
#include <stdexcept>

//============================================================================
// Characterization tests for egolib/game/lighting.c + lighting.h.
//
// WHAT THIS FILE IS
//   Pure characterization. It pins the CURRENT behavior of the lighting
//   substrate that sits under mesh grid lighting, model lighting and particle
//   lighting. Several pinned behaviors are DEFECTS; each one is called out in
//   the test's comment and marked "FINDING". Nothing here asserts what the
//   code *ought* to do. Do not "fix" a production file to make a test here go
//   green -- change the test and the comment together, deliberately.
//
// WHY NO FIXTURE
//   lighting.h's only #include is egolib/Math/_Include.hpp. lighting.c.o lives
//   in libegolib-foundation-base.a and in no other archive. Nothing in this
//   translation unit needs VFS, GL, SDL, EngineContext or a mesh. Bare TEST(),
//   no bootstrap -- same shape as MapTwist.cpp and PhysicsCollisionNormal.cpp.
//
// TWO STANDING TRAPS FOR ANYONE EDITING THIS FILE
//
//   (1) _max_light is a manually-maintained cache and TWO functions gate on it
//       rather than on the contents of _lighting:
//         lighting_cache_base_t::evaluate    (lighting.c:430)
//         lighting_cache_t::lighting_project_cache (lighting.c:197)
//       Neither blend() overload refreshes it. So a fixture that pokes
//       _lighting[...] directly and forgets max_light() silently measures the
//       ambient-only short path. Every fixture below calls max_light()
//       explicitly, and the omission itself is pinned by
//       LightingCacheBase.EvaluateGatesOnStaleMaxLightNotOnContents.
//
//   (2) Out-parameters are pervasive here, and C++ leaves argument evaluation
//       order unspecified. NEVER write EXPECT_EQ(f(..., out), out) -- the macro
//       expands both operands into one expression and the read can race the
//       call. Always store the return value in a local first.
//
// FLOATING-POINT ASSUMPTIONS
//   Several expectations depend on IEEE-754 semantics rather than on lighting
//   logic, and would break under -ffast-math / -Ofast / UBSan
//   float-divide-by-zero (none of which this build uses):
//     - Ego::Math::constrain launders NaN and Inf (see the QUIRK 2 section).
//     - lighting_cache_base_t::blend dispatches on exact == 1.0f / == 0.0f.
//     - dyna_lighting_intensity can emit NaN (see the QUIRK 4/falloff section).
//   If this file starts failing wholesale, check the compiler flags before
//   suspecting lighting.c.
//============================================================================

namespace
{

/// Fill a cache half's seven slots and refresh its _max_light cache.
/// Order matters: max_light() must run AFTER the slots are written.
void setHalf(lighting_cache_base_t& half,
             float px, float mx, float py, float my, float pz, float mz, float amb)
{
    half._lighting[LVEC_PX] = px;
    half._lighting[LVEC_MX] = mx;
    half._lighting[LVEC_PY] = py;
    half._lighting[LVEC_MY] = my;
    half._lighting[LVEC_PZ] = pz;
    half._lighting[LVEC_MZ] = mz;
    half._lighting[LVEC_AMB] = amb;
    half.max_light();
}

void expectVector(const LightingVector& v,
                  float px, float mx, float py, float my, float pz, float mz, float amb)
{
    EXPECT_FLOAT_EQ(v[LVEC_PX], px) << "LVEC_PX";
    EXPECT_FLOAT_EQ(v[LVEC_MX], mx) << "LVEC_MX";
    EXPECT_FLOAT_EQ(v[LVEC_PY], py) << "LVEC_PY";
    EXPECT_FLOAT_EQ(v[LVEC_MY], my) << "LVEC_MY";
    EXPECT_FLOAT_EQ(v[LVEC_PZ], pz) << "LVEC_PZ";
    EXPECT_FLOAT_EQ(v[LVEC_MZ], mz) << "LVEC_MZ";
    EXPECT_FLOAT_EQ(v[LVEC_AMB], amb) << "LVEC_AMB";
}

/// Scope guard for the three module globals. Restores them on destruction so
/// that an early ASSERT_* return, or a throw, cannot leave them perturbed for
/// the rest of a whole-binary run. (gtest_discover_tests gives each test its
/// own process today, so this is belt-and-braces, but the globals are the only
/// shared mutable state this file writes and the discipline should not depend
/// on nothing in between being able to fail.)
struct LightingGlobalsGuard
{
    LightingGlobalsGuard() : a(light_a), d(light_d), nrm(light_nrm) {}
    ~LightingGlobalsGuard() { light_a = a; light_d = d; light_nrm = nrm; }
    LightingGlobalsGuard(const LightingGlobalsGuard&) = delete;
    LightingGlobalsGuard& operator=(const LightingGlobalsGuard&) = delete;

    float a;
    float d;
    Ego::Vector3f nrm;
};

/// Exercises every entry point of lighting.c that produces a number, and
/// returns all of them. Used by the purity test below.
///
/// Coverage note: this reaches lighting_vector_evaluate, lighting_vector_sum,
/// lighting_cache_base_t::{max_light,evaluate}, lighting_cache_t::{max_light,
/// lighting_evaluate_cache, lighting_project_cache, lighting_cache_interpolate},
/// lighting_cache_test, dyna_lighting_intensity, sum_dyna_lighting and
/// dynalight_data_t::init. The three it does NOT reach -- init() on either
/// cache type and the two blend() overloads -- take no inputs beyond their own
/// arguments and produce no value that a global could enter.
std::array<float, 13> purityProbe()
{
    std::array<float, 13> out{};
    out.fill(0.0f);

    LightingVector lvec{{1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f}};
    float dir = 0.0f, amb = 0.0f;
    lighting_vector_evaluate(lvec, Ego::Vector3f(1.0f, -2.0f, 3.0f), dir, amb);
    out[0] = dir;
    out[1] = amb;

    LightingVector acc{};
    acc.fill(0.0f);
    lighting_vector_sum(acc, Ego::Vector3f(1.0f, -2.0f, 0.0f), 10.0f, 3.0f);
    out[2] = acc[LVEC_PX];
    out[3] = acc[LVEC_MY];
    out[4] = acc[LVEC_AMB];

    lighting_cache_t src;
    setHalf(src.low, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f);
    setHalf(src.hgh, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 70.0f);
    src.max_light();
    out[5] = src._max_light;

    float baseAmb = 0.0f;
    out[6] = lighting_cache_base_t::evaluate(src.low, Ego::Vector3f(0.0f, 0.0f, 1.0f), baseAmb);

    const Ego::AxisAlignedBox3f box(Ego::Point3f(0.0f, 0.0f, 0.0f), Ego::Point3f(1.0f, 1.0f, 10.0f));
    out[7] = lighting_cache_t::lighting_evaluate_cache(
        src, Ego::Vector3f(0.0f, 0.0f, 1.0f), 2.5f, box, nullptr, nullptr);

    lighting_cache_t projected;
    lighting_cache_t::lighting_project_cache(projected, src, idlib::identity<Ego::Matrix4f4f>());
    out[8] = projected._max_light;

    lighting_cache_t interpolated;
    const std::array<const lighting_cache_t *, 4> corners{{&src, &src, &src, &src}};
    lighting_cache_t::lighting_cache_interpolate(interpolated, corners, 0.5f, 0.5f);
    out[9] = interpolated.low._lighting[LVEC_PZ];

    lighting_cache_t deltaSrc = src;
    deltaSrc.low._max_delta = 2.0f;
    deltaSrc.hgh._max_delta = 6.0f;
    deltaSrc._max_delta = 4.0f;
    const lighting_cache_t *deltaCorners[4] = {&deltaSrc, nullptr, nullptr, nullptr};
    float lowDelta = 0.0f, hghDelta = 0.0f;
    out[10] = lighting_cache_test(deltaCorners, 0.0f, 0.0f, lowDelta, hghDelta);

    dynalight_data_t dyna;
    dynalight_data_t::init(dyna);
    dyna.level = 1.0f;
    out[11] = dyna_lighting_intensity(&dyna, Ego::Vector3f(100.0f, 0.0f, 0.0f));

    LightingVector dynaAcc{};
    dynaAcc.fill(0.0f);
    sum_dyna_lighting(&dyna, dynaAcc, Ego::Vector3f(100.0f, 0.0f, 0.0f));
    out[12] = dynaAcc[LVEC_AMB];

    return out;
}

} // anonymous namespace

//============================================================================
// SECTION 1 -- layout constants and the module-level globals
//============================================================================

TEST(LightingLayout, DirectionSlotsArePairedAndAmbientIsLast)
{
    // The (plus, minus) pairing on consecutive even/odd indices is load-bearing:
    // lighting_sum_project (lighting.c:365) indexes dir+0 / dir+1 and only ever
    // receives 0, 2 or 4, so each call must land on a pair boundary.
    EXPECT_EQ(LVEC_PX, 0);
    EXPECT_EQ(LVEC_MX, 1);
    EXPECT_EQ(LVEC_PY, 2);
    EXPECT_EQ(LVEC_MY, 3);
    EXPECT_EQ(LVEC_PZ, 4);
    EXPECT_EQ(LVEC_MZ, 5);
    EXPECT_EQ(LVEC_AMB, 6);
    EXPECT_EQ(LIGHTING_VEC_SIZE, 7);

    LightingVector v{};
    EXPECT_EQ(v.size(), static_cast<size_t>(LIGHTING_VEC_SIZE));
}

TEST(LightingLayout, TotalMaxDynaIs64AndIsDefinedInTwoPlaces)
{
    // FINDING (documentation-level, not a behavior bug): TOTAL_MAX_DYNA is
    // #defined twice with the identical replacement token 64 -- at
    // lighting.h:119 and at egolib/egolib_config.h:160. Because the tokens are
    // identical the redefinition is legal and silent, so there are two sources
    // of truth for one constant. Consumers pick up whichever header they
    // happen to include.
    EXPECT_EQ(TOTAL_MAX_DYNA, 64);

    // FINDING: MAXDYNADIST (lighting.h:118) is dead. A grep of egolib, egoboo,
    // tools, cartman, idlib and idlib-game-engine for the identifier finds
    // exactly one occurrence -- the #define itself. Nothing reads it.
    EXPECT_EQ(MAXDYNADIST, 2700);
}

TEST(LightingGlobals, LightingFunctionsDoNotReadLightALightDOrLightNrm)
{
    // light_a / light_d / light_nrm are defined at lighting.c:28-30 but no
    // function in lighting.c reads them; the only writer anywhere is
    // game_wawalite.c (module load), and the readers live in
    // graphic_lighting_dynalist.c and BackgroundRenderPass.cpp.
    //
    // Rather than pin their initial values (which would be fragile against
    // test ordering inside the single gtest process), this pins the PURITY of
    // lighting.c with respect to them: perturb all three, and every number
    // purityProbe() collects must come back EXACTLY equal (EXPECT_EQ on the
    // float values, not the 4-ULP EXPECT_FLOAT_EQ). Catches any future change
    // that folds the global sun into lighting.c's own ambient or direct terms.
    //
    // Scope of the pin: purityProbe() calls every lighting.c entry point that
    // yields a number -- see the coverage note on the helper. init() on either
    // cache type and the two blend() overloads are not in the window; they take
    // no input a global could enter.
    const std::array<float, 13> before = purityProbe();

    {
        LightingGlobalsGuard guard;
        light_a = 123.0f;
        light_d = 456.0f;
        light_nrm = Ego::Vector3f(0.5f, -0.5f, 0.7071f);

        const std::array<float, 13> during = purityProbe();
        for (size_t i = 0; i < during.size(); ++i)
        {
            EXPECT_EQ(during[i], before[i]) << "probe slot " << i;
        }
    }

    // And the guard put them back.
    const std::array<float, 13> after = purityProbe();
    for (size_t i = 0; i < after.size(); ++i)
    {
        EXPECT_EQ(after[i], before[i]) << "probe slot " << i;
    }
}

//============================================================================
// SECTION 2 -- lighting_vector_evaluate / lighting_vector_sum
//
// These two are adjoints of each other. evaluate() GATHERS the six directional
// slots against a normal; sum() SCATTERS a scalar back into them. The pair
// shares one convention -- a positive component reads/writes the P slot, a
// negative component the M slot -- and the round-trip test at the end of this
// section is what pins that convention without transcribing either body.
//============================================================================

TEST(LightingVectorEvaluate, AssignsBothOutParamsRatherThanAccumulating)
{
    // lighting.c:40-41 sets dir = 0 and amb = 0 before doing anything, so both
    // out-params are ASSIGN semantics. Contrast lighting_cache_test in the
    // QUIRK 1 section, whose out-params are accumulate-only -- that asymmetry
    // inside one header is the root of the caller bug pinned there.
    LightingVector lvec{{1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f}};

    float dir = -999.0f, amb = -999.0f;
    lighting_vector_evaluate(lvec, Ego::Vector3f(1.0f, 0.0f, 0.0f), dir, amb);

    EXPECT_FLOAT_EQ(dir, 1.0f);   // LVEC_PX, not -999 + 1
    EXPECT_FLOAT_EQ(amb, 7.0f);   // LVEC_AMB, not -999 + 7
}

TEST(LightingVectorEvaluate, ComponentSignSelectsThePlusOrMinusSlot)
{
    LightingVector lvec{{1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f}};
    float dir = 0.0f, amb = 0.0f;

    // Each axis in isolation, both signs. Any swap of a P/M pair, or any change
    // that makes a component read both slots, moves one of these six numbers.
    lighting_vector_evaluate(lvec, Ego::Vector3f(1.0f, 0.0f, 0.0f), dir, amb);
    EXPECT_FLOAT_EQ(dir, 1.0f);   // LVEC_PX
    lighting_vector_evaluate(lvec, Ego::Vector3f(-1.0f, 0.0f, 0.0f), dir, amb);
    EXPECT_FLOAT_EQ(dir, 2.0f);   // LVEC_MX
    lighting_vector_evaluate(lvec, Ego::Vector3f(0.0f, 1.0f, 0.0f), dir, amb);
    EXPECT_FLOAT_EQ(dir, 3.0f);   // LVEC_PY
    lighting_vector_evaluate(lvec, Ego::Vector3f(0.0f, -1.0f, 0.0f), dir, amb);
    EXPECT_FLOAT_EQ(dir, 4.0f);   // LVEC_MY
    lighting_vector_evaluate(lvec, Ego::Vector3f(0.0f, 0.0f, 1.0f), dir, amb);
    EXPECT_FLOAT_EQ(dir, 5.0f);   // LVEC_PZ
    lighting_vector_evaluate(lvec, Ego::Vector3f(0.0f, 0.0f, -1.0f), dir, amb);
    EXPECT_FLOAT_EQ(dir, 6.0f);   // LVEC_MZ

    // Lighting is strictly one-sided: a -Z normal reads MZ and gets NOTHING
    // from PZ. Pin it, because "both slots contribute" is a plausible mutation.
    LightingVector onlyPz{};
    onlyPz.fill(0.0f);
    onlyPz[LVEC_PZ] = 8.0f;
    lighting_vector_evaluate(onlyPz, Ego::Vector3f(0.0f, 0.0f, -1.0f), dir, amb);
    EXPECT_FLOAT_EQ(dir, 0.0f);
}

TEST(LightingVectorEvaluate, DoesNotNormalizeTheNormalAndSumsAllThreeAxes)
{
    LightingVector lvec{{1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f}};
    float dir = 0.0f, amb = 0.0f;

    // nrm is a raw scale factor, NOT a direction. 2*PX + 3*MY + 4*PZ
    //  = 2*1 + 3*4 + 4*5 = 34. If a future version normalized nrm this would
    // collapse to roughly 34/sqrt(29) and the test fails loudly.
    lighting_vector_evaluate(lvec, Ego::Vector3f(2.0f, -3.0f, 4.0f), dir, amb);
    EXPECT_FLOAT_EQ(dir, 34.0f);
    EXPECT_FLOAT_EQ(amb, 7.0f);

    // Linear in |nrm|: doubling the normal doubles the directed light.
    lighting_vector_evaluate(lvec, Ego::Vector3f(4.0f, -6.0f, 8.0f), dir, amb);
    EXPECT_FLOAT_EQ(dir, 68.0f);
}

TEST(LightingVectorEvaluate, ZeroComponentsContributeNothingAndAmbientStillFlows)
{
    LightingVector lvec{{1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f}};
    float dir = 0.0f, amb = 0.0f;

    // Exactly 0.0f takes neither the > 0 nor the < 0 branch (lighting.c:43-68).
    lighting_vector_evaluate(lvec, Ego::Vector3f(0.0f, 0.0f, 0.0f), dir, amb);
    EXPECT_FLOAT_EQ(dir, 0.0f);
    EXPECT_FLOAT_EQ(amb, 7.0f);   // ambient is unconditional

    // -0.0f also takes neither branch: (-0.0 > 0) and (-0.0 < 0) are both
    // false (lighting.c:43-50).
    //
    // Be honest about what this assertion can see: NOTHING here distinguishes
    // `>` from `>=` or `<` from `<=`. Every branch body multiplies the slot by
    // the component, and the component is +/-0, so all four operator mutants
    // return 0.0f -- and EXPECT_FLOAT_EQ treats -0.0f and +0.0f as equal
    // anyway. What this pins is only that a signed zero is harmless: no NaN,
    // no non-zero contribution, no crash. The comparison operators themselves
    // are pinned by the non-zero cases in
    // ComponentSignSelectsThePlusOrMinusSlot and by the sum/evaluate round
    // trip below.
    lighting_vector_evaluate(lvec, Ego::Vector3f(-0.0f, 0.0f, 0.0f), dir, amb);
    EXPECT_FLOAT_EQ(dir, 0.0f);
    EXPECT_FLOAT_EQ(amb, 7.0f);
}

TEST(LightingVectorSum, AccumulatesIntoTheSignMatchingSlotAndNeverAssigns)
{
    LightingVector lvec{};
    lvec.fill(0.0f);

    // +X writes PX, -Y writes MY (as +30, the magnitude), Z == 0 writes nothing.
    lighting_vector_sum(lvec, Ego::Vector3f(2.0f, -3.0f, 0.0f), 10.0f, 1.0f);
    expectVector(lvec, 20.0f, 0.0f, 0.0f, 30.0f, 0.0f, 0.0f, 1.0f);

    // The whole point: a second call ADDS. If this ever became assignment,
    // graphic_lighting_dynalist.c's per-light accumulation loop would collapse
    // to "last light wins".
    lighting_vector_sum(lvec, Ego::Vector3f(2.0f, -3.0f, 0.0f), 10.0f, 1.0f);
    expectVector(lvec, 40.0f, 0.0f, 0.0f, 60.0f, 0.0f, 0.0f, 2.0f);
}

TEST(LightingVectorSum, AmbientIsAddedUnconditionallyAndIsNotGatedOnTheNormal)
{
    LightingVector lvec{};
    lvec.fill(0.0f);

    // lighting.c:104 is outside every branch. A zero normal contributes no
    // direct light but the ambient still lands. This is exactly the path
    // sum_dyna_lighting relies on (see the QUIRK 3 section).
    lighting_vector_sum(lvec, idlib::zero<Ego::Vector3f>(), 999.0f, 5.0f);
    expectVector(lvec, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 5.0f);
}

TEST(LightingVectorSum, NegativeDirectLightIsStoredUnclamped)
{
    LightingVector lvec{};
    lvec.fill(0.0f);

    // There is no clamp anywhere in lighting.c. Negative direct light is how
    // "blind spots" were expressed historically; do not assert non-negativity.
    lighting_vector_sum(lvec, Ego::Vector3f(0.0f, 0.0f, 1.0f), -8.0f, 0.0f);
    expectVector(lvec, 0.0f, 0.0f, 0.0f, 0.0f, -8.0f, 0.0f, 0.0f);
}

TEST(LightingVectorSum, RoundTripsThroughEvaluateOnAUnitAxis)
{
    // The strongest pin on the shared P/M convention, and the one that does NOT
    // transcribe either implementation: scatter d along an axis, gather it back
    // along the same axis, get d. Any renumbering of the LVEC_* enum, any swap
    // of a P/M pair in either function alone, breaks this.
    const Ego::Vector3f axes[6] = {
        Ego::Vector3f( 1.0f,  0.0f,  0.0f), Ego::Vector3f(-1.0f,  0.0f,  0.0f),
        Ego::Vector3f( 0.0f,  1.0f,  0.0f), Ego::Vector3f( 0.0f, -1.0f,  0.0f),
        Ego::Vector3f( 0.0f,  0.0f,  1.0f), Ego::Vector3f( 0.0f,  0.0f, -1.0f),
    };

    for (const Ego::Vector3f& axis : axes)
    {
        LightingVector lvec{};
        lvec.fill(0.0f);
        lighting_vector_sum(lvec, axis, 8.0f, 0.0f);

        float dir = 0.0f, amb = 0.0f;
        lighting_vector_evaluate(lvec, axis, dir, amb);

        EXPECT_FLOAT_EQ(dir, 8.0f);
        EXPECT_FLOAT_EQ(amb, 0.0f);
    }
}

//============================================================================
// SECTION 3 -- lighting_cache_base_t
//============================================================================

TEST(LightingCacheBase, DefaultConstructionAndInitBothZeroEverything)
{
    lighting_cache_base_t c;
    EXPECT_FLOAT_EQ(c._max_light, 0.0f);
    EXPECT_FLOAT_EQ(c._max_delta, 0.0f);
    expectVector(c._lighting, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    setHalf(c, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f);
    c._max_delta = 42.0f;

    c.init();
    EXPECT_FLOAT_EQ(c._max_light, 0.0f);
    EXPECT_FLOAT_EQ(c._max_delta, 0.0f);
    expectVector(c._lighting, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
}

TEST(LightingCacheBase, MaxLightIsAbsoluteAndDeliberatelyExcludesAmbient)
{
    // lighting.c:119 loops i = 1 .. LIGHTING_VEC_SIZE-2 seeded from
    // |_lighting[0]|, so it covers indices 0..5 -- the six DIRECTIONAL slots --
    // and skips LVEC_AMB (6). The header calls the field "max amplitude of
    // direct light", so the exclusion is intentional; the `- 1` is not an
    // off-by-one waiting to be tidied up.
    lighting_cache_base_t c;
    setHalf(c, 3.0f, 0.0f, 0.0f, 0.0f, 0.0f, -7.0f, 999.0f);
    EXPECT_FLOAT_EQ(c._max_light, 7.0f);   // |-7|, and the 999 ambient ignored

    // Consequence worth its own assertion: an AMBIENT-ONLY cache reports
    // _max_light == 0, which is what flips evaluate() and
    // lighting_project_cache into their short paths (see the next two tests).
    lighting_cache_base_t ambientOnly;
    setHalf(ambientOnly, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1000.0f);
    EXPECT_FLOAT_EQ(ambientOnly._max_light, 0.0f);

    // Idempotent.
    ambientOnly.max_light();
    EXPECT_FLOAT_EQ(ambientOnly._max_light, 0.0f);
}

TEST(LightingCacheBase, BlendKeepOneLeavesLightingCompletelyUntouched)
{
    // lighting.c:130 -- the `1.0f == keep` arm writes NOTHING into _lighting.
    lighting_cache_base_t self, other;
    setHalf(self, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
    setHalf(other, 50.0f, 50.0f, 50.0f, 50.0f, 50.0f, 50.0f, 50.0f);
    self._max_delta = 99.0f;

    lighting_cache_base_t::blend(self, other, 1.0f);

    expectVector(self._lighting, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
    EXPECT_FLOAT_EQ(self._max_delta, 0.0f);   // ASSIGNED to 0, not left at 99
}

TEST(LightingCacheBase, BlendKeepZeroCopiesAllSevenSlotsIncludingAmbient)
{
    // lighting.c:135-146 -- the copy arm runs over i < LIGHTING_VEC_SIZE, so
    // LVEC_AMB is copied too (unlike max_light, which skips it).
    lighting_cache_base_t self, other;
    setHalf(self, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f);
    setHalf(other, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f);

    lighting_cache_base_t::blend(self, other, 0.0f);

    expectVector(self._lighting, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f);
    EXPECT_FLOAT_EQ(self._max_delta, 8.0f);   // max |other - self_old|
}

TEST(LightingCacheBase, BlendLerpsAndReportsTheLargestPerSlotMovement)
{
    // lighting.c:147-156 -- self = self*keep + other*(1-keep).
    lighting_cache_base_t self, other;
    setHalf(self, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f);
    setHalf(other, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f);

    lighting_cache_base_t::blend(self, other, 0.25f);

    // 2*0.25 + 10*0.75 = 8; the movement is |8 - 2| = 6.
    expectVector(self._lighting, 8.0f, 8.0f, 8.0f, 8.0f, 8.0f, 8.0f, 8.0f);
    EXPECT_FLOAT_EQ(self._max_delta, 6.0f);
}

TEST(LightingCacheBase, BlendDoesNotClampKeepAndWillExtrapolate)
{
    // FINDING: `keep` is never range-checked. keep > 1 extrapolates AWAY from
    // `other` and can drive a slot negative. Production feeds
    // local_mesh_lighting_keep, which is either a literal 0.9f or
    // std::pow(0.9f, frame_skip) depending on a #if (graphic_lighting.c:341
    // and :344); both stay within [0,1], so this is latent rather than live --
    // but it is unguarded.
    lighting_cache_base_t self, other;
    setHalf(self, 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    setHalf(other, 10.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    lighting_cache_base_t::blend(self, other, 2.0f);

    EXPECT_FLOAT_EQ(self._lighting[LVEC_PX], -6.0f);   // 2*2 + 10*(-1)
    EXPECT_FLOAT_EQ(self._max_delta, 8.0f);
}

TEST(LightingCacheBase, BlendFastPathDispatchIsExactFloatEqualityNotAnEpsilon)
{
    // FINDING (fragility, current behavior): the "no change" arm is selected by
    // `1.0f == keep` (lighting.c:130), a bit-exact comparison. A keep that is
    // merely very close to 1 takes the full lerp arm instead, doing real work
    // and reporting a tiny non-zero delta. This pins that, so that a future
    // "use an epsilon comparison" cleanup cannot land silently.
    lighting_cache_base_t self, other;
    setHalf(self, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    setHalf(other, 10.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    lighting_cache_base_t::blend(self, other, 0.9999999f);

    EXPECT_GT(self._lighting[LVEC_PX], 0.0f);          // lerp arm ran
    EXPECT_LT(self._lighting[LVEC_PX], 1.0e-4f);       // ...but only barely
    EXPECT_GT(self._max_delta, 0.0f);
    EXPECT_LT(self._max_delta, 1.0e-4f);
}

TEST(LightingCacheBase, BlendNeverRefreshesMaxLight)
{
    // FINDING: blend rewrites _lighting but leaves _max_light stale. Because
    // evaluate() gates on _max_light (lighting.c:430), a caller that blends and
    // then evaluates without an intervening max_light() gets ambient-only
    // light. The one production caller only escapes this by calling
    // pcache_old.max_light() on the line after the blend
    // (graphic_lighting_dynalist.c:375 then :378).
    lighting_cache_base_t self, other;
    setHalf(self, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    setHalf(other, 100.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    lighting_cache_base_t::blend(self, other, 0.0f);

    EXPECT_FLOAT_EQ(self._lighting[LVEC_PX], 100.0f);
    EXPECT_FLOAT_EQ(self._max_light, 1.0f);            // STALE
    self.max_light();
    EXPECT_FLOAT_EQ(self._max_light, 100.0f);
}

TEST(LightingCacheBase, EvaluateGatesOnStaleMaxLightNotOnContents)
{
    // FINDING: evaluate() branches on the CACHED _max_light field
    // (lighting.c:430), not on the contents of _lighting. With a stale zero it
    // returns ambient only and SILENTLY DISCARDS every directional slot -- no
    // error, no clamp, no log. This is the single most likely way for a
    // renderer refactor to lose all directional light on some code path.
    lighting_cache_base_t c;
    c._lighting[LVEC_PZ] = 100.0f;
    c._lighting[LVEC_AMB] = 5.0f;
    // deliberately NOT calling max_light() -- _max_light is still 0
    ASSERT_FLOAT_EQ(c._max_light, 0.0f);

    float amb = -1.0f;
    // Sequence the call before reading `amb`; do not fold both into one macro.
    float total = lighting_cache_base_t::evaluate(c, Ego::Vector3f(0.0f, 0.0f, 1.0f), amb);
    EXPECT_FLOAT_EQ(total, 5.0f);       // the 100 is gone
    EXPECT_FLOAT_EQ(amb, 5.0f);         // amb is ASSIGNED, not accumulated

    c.max_light();
    total = lighting_cache_base_t::evaluate(c, Ego::Vector3f(0.0f, 0.0f, 1.0f), amb);
    EXPECT_FLOAT_EQ(total, 105.0f);     // now the directional slot counts
    EXPECT_FLOAT_EQ(amb, 5.0f);
}

TEST(LightingCacheBase, EvaluateReturnsNegativeTotalForNegativeDirectionalLight)
{
    // max_light() uses std::abs, so a negative slot still opens the gate; the
    // total is then genuinely negative. There is no clamp in this file.
    lighting_cache_base_t c;
    setHalf(c, 0.0f, 0.0f, 0.0f, 0.0f, -100.0f, 0.0f, 5.0f);
    ASSERT_FLOAT_EQ(c._max_light, 100.0f);

    float amb = 0.0f;
    const float total = lighting_cache_base_t::evaluate(c, Ego::Vector3f(0.0f, 0.0f, 1.0f), amb);
    EXPECT_FLOAT_EQ(total, -95.0f);
    EXPECT_FLOAT_EQ(amb, 5.0f);
}

TEST(LightingCacheBase, EvaluateShortPathAgreesWithTheLongPathForAmbientOnlyCaches)
{
    // Because max_light() excludes LVEC_AMB, an ambient-only cache ALWAYS has
    // _max_light == 0 and therefore always takes the short path at
    // lighting.c:430-434 -- even right after a correct max_light() call. That
    // is benign only because the two paths agree when every directional slot
    // is zero.
    //
    // evaluate() cannot be made to take the long path on such a cache, so the
    // agreement is checked by calling the long path's implementation
    // (lighting_vector_evaluate, lighting.c:438) directly on the same data and
    // comparing. If the short path ever stopped forwarding LVEC_AMB, or the
    // long path started treating an all-zero directional set differently, the
    // two would part company here.
    lighting_cache_base_t c;
    setHalf(c, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 9.0f);
    ASSERT_FLOAT_EQ(c._max_light, 0.0f);

    const Ego::Vector3f nrm(1.0f, 1.0f, 1.0f);

    float shortAmb = 0.0f;
    const float shortTotal = lighting_cache_base_t::evaluate(c, nrm, shortAmb);
    EXPECT_FLOAT_EQ(shortTotal, 9.0f);
    EXPECT_FLOAT_EQ(shortAmb, 9.0f);

    float longDir = 0.0f, longAmb = 0.0f;
    lighting_vector_evaluate(c._lighting, nrm, longDir, longAmb);
    const float longTotal = longDir + longAmb;

    EXPECT_FLOAT_EQ(longTotal, shortTotal);
    EXPECT_FLOAT_EQ(longAmb, shortAmb);
}

//============================================================================
// SECTION 4 -- lighting_cache_t (the low/hgh pair)
//
// `low` and `hgh` hold the lighting sampled at the bottom and the top of the
// mesh's Z bounding box; everything downstream lerps between them by height.
//============================================================================

TEST(LightingCacheT, InitResetsBothHalvesAndBothOwnScalars)
{
    lighting_cache_t c;
    setHalf(c.low, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f);
    setHalf(c.hgh, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f);
    c.max_light();
    c._max_delta = 55.0f;
    c.low._max_delta = 3.0f;

    c.init();

    EXPECT_FLOAT_EQ(c._max_light, 0.0f);
    EXPECT_FLOAT_EQ(c._max_delta, 0.0f);
    EXPECT_FLOAT_EQ(c.low._max_light, 0.0f);
    EXPECT_FLOAT_EQ(c.low._max_delta, 0.0f);
    EXPECT_FLOAT_EQ(c.hgh._max_light, 0.0f);
    expectVector(c.low._lighting, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    expectVector(c.hgh._lighting, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
}

TEST(LightingCacheT, MaxLightIsTheMaximumOverBothHalves)
{
    lighting_cache_t c;
    c.low._lighting[LVEC_PX] = 4.0f;
    c.hgh._lighting[LVEC_MZ] = -11.0f;

    c.max_light();

    EXPECT_FLOAT_EQ(c.low._max_light, 4.0f);
    EXPECT_FLOAT_EQ(c.hgh._max_light, 11.0f);
    EXPECT_FLOAT_EQ(c._max_light, 11.0f);
}

TEST(LightingCacheT, BlendPropagatesMaxDeltaButLeavesMaxLightStale)
{
    // Same finding as LightingCacheBase.BlendNeverRefreshesMaxLight, one level
    // up: _max_delta IS assigned (lighting.c:185) but _max_light is not touched
    // by either blend overload.
    //
    // Note the API wart while we are here: lighting_cache_t::blend takes
    // `other` by NON-const reference (lighting.h:109) even though the body only
    // reads it, whereas lighting_cache_base_t::blend takes it const
    // (lighting.h:81). That is why `other` below cannot be const.
    lighting_cache_t self, other;
    setHalf(self.low, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    setHalf(self.hgh, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    setHalf(other.low, 11.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    setHalf(other.hgh, 101.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    self.max_light();
    ASSERT_FLOAT_EQ(self._max_light, 1.0f);

    lighting_cache_t::blend(self, other, 0.0f);

    EXPECT_FLOAT_EQ(self.low._lighting[LVEC_PX], 11.0f);
    EXPECT_FLOAT_EQ(self.hgh._lighting[LVEC_PX], 101.0f);
    EXPECT_FLOAT_EQ(self.low._max_delta, 10.0f);
    EXPECT_FLOAT_EQ(self.hgh._max_delta, 100.0f);
    EXPECT_FLOAT_EQ(self._max_delta, 100.0f);     // max of the two halves
    EXPECT_FLOAT_EQ(self._max_light, 1.0f);       // STALE, callers must redo it

    self.max_light();
    EXPECT_FLOAT_EQ(self._max_light, 101.0f);

    // blend really does not mutate `other`, despite the non-const signature.
    EXPECT_FLOAT_EQ(other.low._lighting[LVEC_PX], 11.0f);
}

//============================================================================
// SECTION 5 -- lighting_cache_t::lighting_project_cache
//
// Rotates a world-space lighting cache into an object's local frame. This is
// also the ONLY way to characterize the file-static lighting_sum_project.
//
// QUIRK 5, resolved honestly: lighting_sum_project rejects any `dir` outside
// {0, 2, 4} (lighting.c:367). That guard is UNREACHABLE and its bool return is
// DEAD. The function is forward-declared `static` at lighting.c:34 (the
// definition at :365 omits the keyword, but the earlier declaration governs),
// so it has internal linkage -- `nm` reports it as a local `t` symbol in
// libegolib-foundation-base.a and no test can name it. Its only three call
// sites are lighting.c:211-213, which pass the integer literals 0, 2 and 4 and
// discard the result. There is therefore nothing to assert about the guard;
// what CAN be pinned is the axis permutation the three calls produce, below.
//============================================================================

TEST(LightingCacheProjection, IdentityMatrixPermutesAxesRatherThanPreservingThem)
{
    // Hand-derived from Math/Standard.cpp:77-90 plus lighting.c:211-213.
    // For the identity matrix:
    //   right = mat_getChrRight   = column 1          = ( 0, 1, 0) -> slot pair 0
    //   fwd   = mat_getChrForward = NEGATED column 0  = (-1,-0,-0) -> slot pair 2
    //   up    = mat_getChrUp      = column 2          = ( 0, 0, 1) -> slot pair 4
    // Feeding those through lighting_sum_project gives the map
    //   world PX -> local MY    world MX -> local PY
    //   world PY -> local PX    world MY -> local MX
    //   world PZ -> local PZ    world MZ -> local MZ
    // i.e. the identity matrix is NOT an identity projection. The X/Y swap is
    // the engine's character-space convention; the extra P/M flip on the Y pair
    // comes from mat_getChrForward's negation.
    lighting_cache_t src, dst;
    setHalf(src.low, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f);
    setHalf(src.hgh, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 70.0f);
    src.max_light();

    lighting_cache_t::lighting_project_cache(dst, src, idlib::identity<Ego::Matrix4f4f>());

    expectVector(dst.low._lighting,
                 /*PX*/ 3.0f, /*MX*/ 4.0f, /*PY*/ 2.0f, /*MY*/ 1.0f,
                 /*PZ*/ 5.0f, /*MZ*/ 6.0f, /*AMB*/ 7.0f);
    expectVector(dst.hgh._lighting,
                 30.0f, 40.0f, 20.0f, 10.0f, 50.0f, 60.0f, 70.0f);

    // max_light() is re-derived on the destination (lighting.c:216) and still
    // excludes ambient, so 70 does not win.
    EXPECT_FLOAT_EQ(dst._max_light, 60.0f);
    // _max_delta is zeroed by dst.init() and never derived from src.
    EXPECT_FLOAT_EQ(dst._max_delta, 0.0f);
}

TEST(LightingCacheProjection, NegatedBasisExercisesTheOppositeSignArmsOfTheProjection)
{
    // Companion to the identity case above, and the reason it exists: the
    // identity basis is right = (0,1,0), fwd = (-1,-0,-0), up = (0,0,1), so it
    // only ever runs lighting_sum_project's `vec[kY] > 0` (lighting.c:387),
    // `vec[kX] < 0` (:378) and `vec[kZ] > 0` (:404) arms. Half of the function
    // -- the three opposite-sign arms, which carry the REVERSED P/M mapping --
    // would otherwise never execute in this suite, and swapping their dir+0 /
    // dir+1 targets would go unnoticed.
    //
    // Negating the 3x3 block flips all three basis vectors:
    //   right = column 1          = ( 0,-1, 0) -> :395 (kY < 0), slot pair 0
    //   fwd   = NEGATED column 0  = ( 1, 0, 0) -> :370 (kX > 0), slot pair 2
    //   up    = column 2          = ( 0, 0,-1) -> :412 (kZ < 0), slot pair 4
    // Each still has unit length, so nothing throws. The negative arms use
    // `-=` against the OPPOSITE source slot, so with src.low = 1..7:
    //   PX <- -(-1)*MY = 4   MX <- -(-1)*PY = 3     (from right)
    //   PY <-  (+1)*PX = 1   MY <-  (+1)*MX = 2     (from fwd)
    //   PZ <- -(-1)*MZ = 6   MZ <- -(-1)*PZ = 5     (from up)
    lighting_cache_t src, dst;
    setHalf(src.low, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f);
    setHalf(src.hgh, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 70.0f);
    src.max_light();

    Ego::Matrix4f4f negated = idlib::identity<Ego::Matrix4f4f>();
    negated(0, 0) = -1.0f;
    negated(1, 1) = -1.0f;
    negated(2, 2) = -1.0f;

    lighting_cache_t::lighting_project_cache(dst, src, negated);

    expectVector(dst.low._lighting,
                 /*PX*/ 4.0f, /*MX*/ 3.0f, /*PY*/ 1.0f, /*MY*/ 2.0f,
                 /*PZ*/ 6.0f, /*MZ*/ 5.0f, /*AMB*/ 7.0f);
    expectVector(dst.hgh._lighting,
                 40.0f, 30.0f, 10.0f, 20.0f, 60.0f, 50.0f, 70.0f);

    EXPECT_FLOAT_EQ(dst._max_light, 60.0f);
}

TEST(LightingCacheProjection, DestinationIsFullyWipedBeforeAnythingIsWritten)
{
    // lighting.c:191 calls dst.init() first, so dst is a pure output parameter,
    // never an accumulator -- even on the early-return path below.
    lighting_cache_t src, dst;
    setHalf(dst.low, 999.0f, 999.0f, 999.0f, 999.0f, 999.0f, 999.0f, 999.0f);
    dst._max_delta = 77.0f;
    dst._max_light = 55.0f;

    setHalf(src.low, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 7.0f);
    setHalf(src.hgh, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 8.0f);
    src.max_light();
    ASSERT_FLOAT_EQ(src._max_light, 0.0f);   // ambient-only -> early return

    lighting_cache_t::lighting_project_cache(dst, src, idlib::identity<Ego::Matrix4f4f>());

    // Ambient is copied verbatim BEFORE the early return (lighting.c:194-195),
    // and is never rotated by the matrix.
    EXPECT_FLOAT_EQ(dst.low._lighting[LVEC_AMB], 7.0f);
    EXPECT_FLOAT_EQ(dst.hgh._lighting[LVEC_AMB], 8.0f);
    EXPECT_FLOAT_EQ(dst.low._lighting[LVEC_PX], 0.0f);
    EXPECT_FLOAT_EQ(dst._max_delta, 0.0f);
    EXPECT_FLOAT_EQ(dst._max_light, 0.0f);
}

TEST(LightingCacheProjection, StaleSourceMaxLightSilentlyDegradesToAmbientOnly)
{
    // FINDING: lighting_project_cache early-returns on src._max_light == 0.0f
    // (lighting.c:197), the CACHED field again. Populate src._lighting and
    // forget max_light() and every directional slot is dropped without a word.
    lighting_cache_t src, dst;
    src.low._lighting[LVEC_PX] = 50.0f;
    src.low._lighting[LVEC_AMB] = 1.0f;
    // deliberately no max_light()
    ASSERT_FLOAT_EQ(src._max_light, 0.0f);

    lighting_cache_t::lighting_project_cache(dst, src, idlib::identity<Ego::Matrix4f4f>());
    EXPECT_FLOAT_EQ(dst.low._lighting[LVEC_AMB], 1.0f);
    EXPECT_FLOAT_EQ(dst.low._lighting[LVEC_MY], 0.0f);   // where PX would land

    src.max_light();
    lighting_cache_t::lighting_project_cache(dst, src, idlib::identity<Ego::Matrix4f4f>());
    EXPECT_FLOAT_EQ(dst.low._lighting[LVEC_MY], 50.0f);  // now it appears
}

TEST(LightingCacheProjection, UniformScaleIsErasedBecauseTheBasisIsNormalized)
{
    // lighting.c:206-208 normalize all three basis vectors, so any uniform
    // scale baked into the object matrix contributes nothing. A five-times
    // larger object is not five times brighter.
    lighting_cache_t src, identityDst, scaledDst;
    setHalf(src.low, 10.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    src.max_light();

    Ego::Matrix4f4f scaled = idlib::identity<Ego::Matrix4f4f>();
    scaled(0, 0) = 5.0f;
    scaled(1, 1) = 5.0f;
    scaled(2, 2) = 5.0f;

    lighting_cache_t::lighting_project_cache(identityDst, src, idlib::identity<Ego::Matrix4f4f>());
    lighting_cache_t::lighting_project_cache(scaledDst, src, scaled);

    EXPECT_FLOAT_EQ(identityDst.low._lighting[LVEC_MY], 10.0f);
    for (size_t i = 0; i < LIGHTING_VEC_SIZE; ++i)
    {
        EXPECT_FLOAT_EQ(scaledDst.low._lighting[i], identityDst.low._lighting[i]) << "slot " << i;
    }
}

TEST(LightingCacheProjection, ThrowsDomainErrorOnAMatrixWithACollapsedBasisAxis)
{
    // FINDING: lighting.c:206-208 use Ego::normalize(v).get_vector(), and
    // idlib::normalization_result::get_vector() throws
    // std::domain_error("unable to normalize zero vector") when the length is
    // zero (idlib/idlib-math/.../math/vector.hpp -- the non-throwing
    // get_vector_or_default() sits right next to it and is not used).
    //
    // idlib::matrix's default constructor zero-fills every element, so a
    // DEFAULT-CONSTRUCTED Ego::Matrix4f4f is a zero matrix, not an identity,
    // and trips this immediately. The throw propagates out of a per-frame
    // render path (ObjectGraphics / ParticleGraphics call this) with no handler
    // in lighting.c.
    lighting_cache_t src, dst;
    setHalf(src.low, 10.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    src.max_light();
    ASSERT_NE(src._max_light, 0.0f);

    EXPECT_THROW(lighting_cache_t::lighting_project_cache(dst, src, Ego::Matrix4f4f()),
                 std::domain_error);

    // A single collapsed axis is enough -- it does not take a fully zero matrix.
    Ego::Matrix4f4f collapsedRight = idlib::identity<Ego::Matrix4f4f>();
    collapsedRight(0, 1) = 0.0f;
    collapsedRight(1, 1) = 0.0f;
    collapsedRight(2, 1) = 0.0f;
    EXPECT_THROW(lighting_cache_t::lighting_project_cache(dst, src, collapsedRight),
                 std::domain_error);
}

TEST(LightingCacheProjection, EarlyReturnOrderingIsWhatKeepsUnlitObjectsFromThrowing)
{
    // The src._max_light == 0 early return at lighting.c:197 sits ABOVE the
    // normalize calls at :206-208. That ordering is the only thing that stops
    // an unlit object with a degenerate matrix from throwing out of the render
    // loop. Pin it: moving the normalization above the early return would turn
    // every unlit zero-scaled object into an exception.
    lighting_cache_t src, dst;
    setHalf(src.low, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 3.0f);
    src.max_light();
    ASSERT_FLOAT_EQ(src._max_light, 0.0f);

    EXPECT_NO_THROW(lighting_cache_t::lighting_project_cache(dst, src, Ego::Matrix4f4f()));
    EXPECT_FLOAT_EQ(dst.low._lighting[LVEC_AMB], 3.0f);
}

//============================================================================
// SECTION 6 -- lighting_cache_t::lighting_cache_interpolate
//
// Bilinear blend of up to four tile-corner caches. Corner/weight mapping,
// verified against the caller in graphic_lighting.c: `u` is the +X fraction
// inside the tile and `v` the +Y fraction; src[0] = (ix,iy), src[1] = (ix+1,iy),
// src[2] = (ix,iy+1), src[3] = (ix+1,iy+1).
//============================================================================

namespace
{

/// Build four corner caches whose low[LVEC_PX] values are 1, 2, 3, 4.
struct FourCorners
{
    lighting_cache_t c[4];
    std::array<const lighting_cache_t *, 4> ptr;

    FourCorners()
    {
        for (int i = 0; i < 4; ++i)
        {
            setHalf(c[i].low, static_cast<float>(i + 1), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
            setHalf(c[i].hgh, static_cast<float>(10 * (i + 1)), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
            c[i].max_light();
            ptr[i] = &c[i];
        }
    }
};

} // anonymous namespace

TEST(LightingCacheInterpolation, EachCornerIsSelectedExactlyByItsOwnUvExtreme)
{
    FourCorners f;
    lighting_cache_t dst;

    struct { float u, v, expected; } cases[] = {
        {0.0f, 0.0f, 1.0f},   // src[0], weight (1-u)(1-v)
        {1.0f, 0.0f, 2.0f},   // src[1], weight u(1-v)
        {0.0f, 1.0f, 3.0f},   // src[2], weight (1-u)v
        {1.0f, 1.0f, 4.0f},   // src[3], weight uv
    };

    for (const auto& c : cases)
    {
        const bool ok = lighting_cache_t::lighting_cache_interpolate(dst, f.ptr, c.u, c.v);
        EXPECT_TRUE(ok);
        EXPECT_FLOAT_EQ(dst.low._lighting[LVEC_PX], c.expected) << "u=" << c.u << " v=" << c.v;
        EXPECT_FLOAT_EQ(dst.hgh._lighting[LVEC_PX], c.expected * 10.0f);
    }

    // Midpoint: every weight is 0.25, so this is the plain mean.
    const bool ok = lighting_cache_t::lighting_cache_interpolate(dst, f.ptr, 0.5f, 0.5f);
    EXPECT_TRUE(ok);
    EXPECT_FLOAT_EQ(dst.low._lighting[LVEC_PX], 2.5f);
    EXPECT_FLOAT_EQ(dst._max_light, 25.0f);   // recomputed from hgh's 25
}

TEST(LightingCacheInterpolation, ClampsUAndVIntoTheUnitSquare)
{
    FourCorners f;
    lighting_cache_t dst;

    // lighting.c:223-224 constrain both to [0,1], so (-5, 7) behaves as (0, 1).
    const bool ok = lighting_cache_t::lighting_cache_interpolate(dst, f.ptr, -5.0f, 7.0f);
    EXPECT_TRUE(ok);
    EXPECT_FLOAT_EQ(dst.low._lighting[LVEC_PX], 3.0f);   // == the (0,1) corner
}

TEST(LightingCacheInterpolation, RenormalizesWhenSomeCornersAreMissing)
{
    // The only interesting line in the function (lighting.c:272-281): partial
    // weight coverage is scaled back up to full strength, so a mesh-edge tile
    // does not go dark.
    //
    // (The source also skips the division outright when wt_sum is EXACTLY
    // 1.0f, at lighting.c:274. That is a real branch but it is an optimization
    // only -- dividing by 1.0f is a numeric no-op -- so no assertion in this
    // file can distinguish its presence from its absence. Verified by mutation:
    // replacing the condition with `true` leaves every test in this file
    // green. Noted as a source fact, not as a pinned behavior.)
    FourCorners f;
    lighting_cache_t dst;
    std::array<const lighting_cache_t *, 4> partial = {f.ptr[0], nullptr, nullptr, nullptr};

    const bool ok = lighting_cache_t::lighting_cache_interpolate(dst, partial, 0.5f, 0.5f);
    EXPECT_TRUE(ok);
    // raw = 1 * 0.25, wt_sum = 0.25, so the result is the full 1.0 not 0.25.
    EXPECT_FLOAT_EQ(dst.low._lighting[LVEC_PX], 1.0f);
}

TEST(LightingCacheInterpolation, ReturnsFalseAndLeavesDestinationZeroedWhenAllCornersAreNull)
{
    lighting_cache_t dst;
    setHalf(dst.low, 42.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    std::array<const lighting_cache_t *, 4> none = {nullptr, nullptr, nullptr, nullptr};

    const bool ok = lighting_cache_t::lighting_cache_interpolate(dst, none, 0.5f, 0.5f);

    EXPECT_FALSE(ok);
    // dst.init() at lighting.c:221 runs before the null checks, so the 42 is
    // gone even though nothing was interpolated into its place.
    EXPECT_FLOAT_EQ(dst.low._lighting[LVEC_PX], 0.0f);
    EXPECT_FLOAT_EQ(dst._max_light, 0.0f);
}

TEST(LightingCacheInterpolation, ANonNullCornerWithZeroWeightStillCountsAsNoCoverage)
{
    // Subtle rejection path: the return value is `wt_sum > 0.0f`, NOT "at least
    // one corner was non-null". src[3]'s weight is u*v, which is 0 at the
    // origin of the tile, so a perfectly valid corner cache yields false and a
    // zeroed destination -- and max_light() never runs.
    FourCorners f;
    lighting_cache_t dst;
    std::array<const lighting_cache_t *, 4> onlyFar = {nullptr, nullptr, nullptr, f.ptr[3]};

    const bool ok = lighting_cache_t::lighting_cache_interpolate(dst, onlyFar, 0.0f, 0.0f);

    EXPECT_FALSE(ok);
    EXPECT_FLOAT_EQ(dst.low._lighting[LVEC_PX], 0.0f);
    EXPECT_FLOAT_EQ(dst._max_light, 0.0f);
}

TEST(LightingCacheInterpolation, InterpolatesAmbientButDoesNotPropagateMaxDelta)
{
    // Ambient IS part of the blend (the loops run over all LIGHTING_VEC_SIZE
    // slots), but _max_delta is not: dst.init() zeroes it and nothing in
    // lighting_cache_interpolate ever writes it, no matter what the sources
    // carried. A caller that expects an interpolated delta gets 0.
    lighting_cache_t a, b, dst;
    setHalf(a.low, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 4.0f);
    setHalf(b.low, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 8.0f);
    a._max_delta = 77.0f;
    a.low._max_delta = 77.0f;
    b._max_delta = 88.0f;
    std::array<const lighting_cache_t *, 4> src = {&a, &b, nullptr, nullptr};

    const bool ok = lighting_cache_t::lighting_cache_interpolate(dst, src, 0.5f, 0.0f);

    EXPECT_TRUE(ok);
    EXPECT_FLOAT_EQ(dst.low._lighting[LVEC_AMB], 6.0f);   // (4 + 8) / 2
    EXPECT_FLOAT_EQ(dst._max_delta, 0.0f);
    EXPECT_FLOAT_EQ(dst.low._max_delta, 0.0f);
}

//============================================================================
// SECTION 7 -- lighting_cache_test  ***QUIRK 1***
//
// This is the important section. lighting_cache_test estimates how much the
// lighting at (u,v) has changed, by bilinearly blending the four corners'
// cached _max_delta values.
//
//   The RETURN value is computed into a fresh local zeroed at lighting.c:300 --
//   it is well defined no matter what the caller does.
//
//   The two REFERENCE out-params are NEVER zeroed. They are `+=`-accumulated at
//   lighting.c:315-316, 326-327, 337-338 and 348-349, and then DIVIDED IN PLACE
//   by wt_sum at :357-358. Whatever the caller passes in is folded into the
//   result AND renormalized along with it.
//
// FINDING -- LIVE UNINITIALIZED READ IN A DIFFERENT FILE. DO NOT FIX HERE.
//   graphic_lighting.c:180 declares `float low_delta, hgh_delta;` with no
//   initializer and passes them straight into this function at :181 (through
//   GridIllumination::grid_lighting_test, which forwards them verbatim). So the
//   uninitialized stack values are what gets accumulated into and scaled.
//   graphic_lighting.c:181 does assign the clean return value to `pdelta`, but
//   :189 OVERWRITES pdelta with `low_wt*low_delta + hgh_wt*hgh_delta` -- the
//   one well-defined number is discarded and the contaminated one kept. From
//   there it flows into test_corners, is divided by the corner light and
//   constrain-clamped to [0,10] (which bounds but does not remove the
//   corruption), accumulates into the PERSISTENT per-tile field
//   tile._vertexLightingCache._d1_cache[corner] at :220, and drives the
//   `pdelta > threshold` test at :222 that sets retval at :224. That retval
//   reaches light_fans_throttle_update at :289, which is
//   compiled in for shipped builds -- egolib_config.h:194-195 does
//   `#define CLIP_LIGHT_FANS` and `#undef CLIP_ALL_LIGHT_FANS`. It gates the
//   per-frame "should I relight this tile" decision.
//
//   Contributing factor: lighting.h:140 names the parameters `low_max_diff` /
//   `hgh_max_diff`, which reads like an assign-out contract, while the
//   definition at lighting.c:291 names them `low_delta` / `hgh_delta`. A caller
//   reading only the header would write exactly the bug at graphic_lighting.c:180.
//
//   The tests below pin the lighting.c half of this, which needs no mesh. The
//   caller-side fix belongs to a separate, deliberate behavior-changing pass.
//============================================================================

namespace
{

/// Four caches carrying only delta values: _max_delta 1..4, low 10..40, hgh 100..400.
struct FourDeltas
{
    lighting_cache_t c[4];
    const lighting_cache_t *ptr[4];

    FourDeltas()
    {
        for (int i = 0; i < 4; ++i)
        {
            c[i]._max_delta = static_cast<float>(i + 1);
            c[i].low._max_delta = static_cast<float>(10 * (i + 1));
            c[i].hgh._max_delta = static_cast<float>(100 * (i + 1));
            ptr[i] = &c[i];
        }
    }
};

} // anonymous namespace

TEST(LightingCacheTestFn, ReturnValueIsACleanLocalAndTracksTheCornerMaxDeltas)
{
    // The return value alone is trustworthy: `delta` is zeroed at
    // lighting.c:300 regardless of the caller. Sweep the four corners.
    FourDeltas f;

    struct { float u, v, expected; } cases[] = {
        {0.0f, 0.0f, 1.0f},
        {1.0f, 0.0f, 2.0f},
        {0.0f, 1.0f, 3.0f},
        {1.0f, 1.0f, 4.0f},
    };

    for (const auto& c : cases)
    {
        float low = 0.0f, hgh = 0.0f;
        const float delta = lighting_cache_test(f.ptr, c.u, c.v, low, hgh);
        EXPECT_FLOAT_EQ(delta, c.expected) << "u=" << c.u << " v=" << c.v;
    }
}

TEST(LightingCacheTestFn, QUIRK1_OutParamsAccumulateIntoTheCallersValueAndAreNeverZeroed)
{
    // ***QUIRK 1, the core pin.*** Seed the two reference parameters with
    // sentinels and watch them survive into the result. If a future version
    // ever assigned instead of accumulating, both numbers below drop by exactly
    // the seed -- which is precisely the change that would fix
    // graphic_lighting.c:180's uninitialized read, and precisely the change
    // this test is here to make visible rather than silent.
    FourDeltas f;

    // Control: zero-seeded, the "intended" answer at the tile midpoint.
    float lowClean = 0.0f, hghClean = 0.0f;
    const float deltaClean = lighting_cache_test(f.ptr, 0.5f, 0.5f, lowClean, hghClean);
    EXPECT_FLOAT_EQ(deltaClean, 2.5f);                 // 0.25*(1+2+3+4)
    EXPECT_FLOAT_EQ(lowClean, 25.0f);                  // 0.25*(10+20+30+40)
    EXPECT_FLOAT_EQ(hghClean, 250.0f);                 // 0.25*(100+200+300+400)

    // Same call, seeded references. wt_sum is exactly 1 here, so the seed is
    // added straight through.
    float lowSeeded = 1000.0f, hghSeeded = 5000.0f;
    const float deltaSeeded = lighting_cache_test(f.ptr, 0.5f, 0.5f, lowSeeded, hghSeeded);
    EXPECT_FLOAT_EQ(deltaSeeded, 2.5f);                // return value unaffected
    EXPECT_FLOAT_EQ(lowSeeded, 1025.0f);               // 1000 + 25
    EXPECT_FLOAT_EQ(hghSeeded, 5250.0f);               // 5000 + 250
}

TEST(LightingCacheTestFn, QUIRK1_CallerGarbageIsAmplifiedWhenCornersAreMissing)
{
    // ***QUIRK 1, the part that makes it worse than "the garbage passes
    // through".*** The `/= wt_sum` at lighting.c:357-358 divides the WHOLE
    // accumulator, caller seed included. On a mesh-edge tile some of
    // graphic_lighting.c's four fan[] lookups come back Index1D::Invalid and
    // the corresponding cache pointers are null, so wt_sum < 1 and the
    // caller's uninitialized stack value is SCALED UP by 1/wt_sum -- by up to
    // 4x at a tile midpoint with a single valid corner.
    FourDeltas f;
    const lighting_cache_t *oneCorner[4] = {f.ptr[0], nullptr, nullptr, nullptr};

    // wt_sum = (1-0.5)*(1-0.5) = 0.25.
    float low = 8.0f;    // stand-in for uninitialized stack
    float hgh = 0.0f;    // control: correctly zeroed
    const float delta = lighting_cache_test(oneCorner, 0.5f, 0.5f, low, hgh);

    EXPECT_FLOAT_EQ(delta, 1.0f);     // (0.25 * 1) / 0.25 -- return still clean
    EXPECT_FLOAT_EQ(hgh, 100.0f);     // (0 + 0.25*100) / 0.25 -- correct
    EXPECT_FLOAT_EQ(low, 42.0f);      // (8 + 0.25*10) / 0.25 -- should be 40
}

TEST(LightingCacheTestFn, NullArrayPointerReturnsZeroAndLeavesOutParamsCompletelyUntouched)
{
    // lighting.c:302 returns before touching either reference. Note this is a
    // THIRD out-param discipline in the same function: assigned nowhere,
    // accumulated on the normal path, untouched on the rejection paths.
    float low = 111.0f, hgh = 222.0f;
    const float delta = lighting_cache_test(nullptr, 0.5f, 0.5f, low, hgh);

    EXPECT_FLOAT_EQ(delta, 0.0f);
    EXPECT_FLOAT_EQ(low, 111.0f);
    EXPECT_FLOAT_EQ(hgh, 222.0f);
}

TEST(LightingCacheTestFn, AllFourCornersNullReturnsZeroAndSkipsTheDivisionEntirely)
{
    // wt_sum stays 0, so the `if (wt_sum > 0.0f)` block at lighting.c:354 never
    // runs and the seeds are not even divided. There is no length check on the
    // array -- only the pointer itself and its four elements are tested -- so
    // passing anything shorter than four entries is undefined behavior.
    const lighting_cache_t *none[4] = {nullptr, nullptr, nullptr, nullptr};
    float low = 777.0f, hgh = 888.0f;
    const float delta = lighting_cache_test(none, 0.5f, 0.5f, low, hgh);

    EXPECT_FLOAT_EQ(delta, 0.0f);
    EXPECT_FLOAT_EQ(low, 777.0f);
    EXPECT_FLOAT_EQ(hgh, 888.0f);
}

TEST(LightingCacheTestFn, ClampsUAndVIntoTheUnitSquare)
{
    FourDeltas f;
    float low = 0.0f, hgh = 0.0f;
    const float delta = lighting_cache_test(f.ptr, -3.0f, 9.0f, low, hgh);
    EXPECT_FLOAT_EQ(delta, 3.0f);      // behaves as u=0, v=1 -> corner 2
    EXPECT_FLOAT_EQ(low, 30.0f);
}

//============================================================================
// SECTION 8 -- lighting_cache_t::lighting_evaluate_cache  ***QUIRK 2***
//
// Height-lerps the low/hgh halves and evaluates each against a normal. `z` is
// an absolute world Z and `bbox` is the whole-MESH Z extent (callers pass
// mesh->_tmem._bbox), not the object's own box.
//============================================================================

namespace
{

/// low = {PZ 40, AMB 4} -> evaluates to 44 along +Z; hgh = {PZ 80, AMB 8} -> 88.
lighting_cache_t makeHeightCache()
{
    lighting_cache_t c;
    setHalf(c.low, 0.0f, 0.0f, 0.0f, 0.0f, 40.0f, 0.0f, 4.0f);
    setHalf(c.hgh, 0.0f, 0.0f, 0.0f, 0.0f, 80.0f, 0.0f, 8.0f);
    c.max_light();
    return c;
}

} // anonymous namespace

TEST(LightingEvaluateCache, LerpsLinearlyBetweenTheLowAndHighHalvesAndClampsOutsideTheBox)
{
    const lighting_cache_t c = makeHeightCache();
    const Ego::AxisAlignedBox3f box(Ego::Point3f(0.0f, 0.0f, 0.0f), Ego::Point3f(1.0f, 1.0f, 10.0f));
    const Ego::Vector3f up(0.0f, 0.0f, 1.0f);

    struct { float z, tot, amb, dir; } cases[] = {
        {  0.0f, 44.0f, 4.0f, 40.0f},
        {  2.5f, 55.0f, 5.0f, 50.0f},
        {  5.0f, 66.0f, 6.0f, 60.0f},
        { 10.0f, 88.0f, 8.0f, 80.0f},
        { 20.0f, 88.0f, 8.0f, 80.0f},   // clamped to the top of the box
        {-20.0f, 44.0f, 4.0f, 40.0f},   // clamped to the bottom
    };

    for (const auto& t : cases)
    {
        float amb = -1.0f, dir = -1.0f;
        const float tot = lighting_cache_t::lighting_evaluate_cache(c, up, t.z, box, &amb, &dir);
        EXPECT_FLOAT_EQ(tot, t.tot) << "z=" << t.z;
        EXPECT_FLOAT_EQ(amb, t.amb) << "z=" << t.z;
        EXPECT_FLOAT_EQ(dir, t.dir) << "z=" << t.z;
    }
}

TEST(LightingEvaluateCache, HeightWeightIsRelativeToTheBoxMinimumNotToAbsoluteZero)
{
    // lighting.c:456 is
    //     hgh_wt = (z - bbox.min.z) / (bbox.max.z - bbox.min.z)
    // and the `- bbox.min.z` terms only matter when the box does not start at
    // zero. Production always passes mesh->_tmem._bbox (graphic_lighting.c,
    // ObjectGraphics.cpp, ParticleGraphics.cpp), whose minimum Z is the mesh
    // floor and is generally NOT zero, so this is the case that actually ships.
    //
    // A box lifted 100 units off the origin must reproduce the same lerp as
    // the origin box in the sibling test: any mutation to `z / max`,
    // `(z - min) / max` or `z / (max - min)` changes at least one of these.
    const lighting_cache_t c = makeHeightCache();
    const Ego::AxisAlignedBox3f offset(Ego::Point3f(0.0f, 0.0f, 100.0f),
                                       Ego::Point3f(1.0f, 1.0f, 110.0f));
    const Ego::Vector3f up(0.0f, 0.0f, 1.0f);

    struct { float z, tot, amb, dir; } cases[] = {
        {100.0f, 44.0f, 4.0f, 40.0f},   // at the floor -> pure low half
        {102.5f, 55.0f, 5.0f, 50.0f},
        {105.0f, 66.0f, 6.0f, 60.0f},   // midpoint
        {110.0f, 88.0f, 8.0f, 80.0f},   // at the ceiling -> pure hgh half
        {  0.0f, 44.0f, 4.0f, 40.0f},   // BELOW the box -> clamped to low
        {200.0f, 88.0f, 8.0f, 80.0f},   // above the box -> clamped to hgh
    };

    for (const auto& t : cases)
    {
        float amb = -1.0f, dir = -1.0f;
        const float tot = lighting_cache_t::lighting_evaluate_cache(c, up, t.z, offset, &amb, &dir);
        EXPECT_FLOAT_EQ(tot, t.tot) << "z=" << t.z;
        EXPECT_FLOAT_EQ(amb, t.amb) << "z=" << t.z;
        EXPECT_FLOAT_EQ(dir, t.dir) << "z=" << t.z;
    }
}

TEST(LightingEvaluateCache, BothOutParamsAreAssignedNotAccumulated)
{
    // lighting.c:462-463 zero both before use, so pre-seeded sentinels vanish.
    // This is the direct contrast with lighting_cache_test in SECTION 7: two
    // functions in the same header, both taking "delta/light" out-params, with
    // opposite disciplines and no naming to tell them apart.
    const lighting_cache_t c = makeHeightCache();
    const Ego::AxisAlignedBox3f box(Ego::Point3f(0.0f, 0.0f, 0.0f), Ego::Point3f(1.0f, 1.0f, 10.0f));

    float amb = 1000.0f, dir = 2000.0f;
    const float tot = lighting_cache_t::lighting_evaluate_cache(
        c, Ego::Vector3f(0.0f, 0.0f, 1.0f), 0.0f, box, &amb, &dir);

    EXPECT_FLOAT_EQ(tot, 44.0f);
    EXPECT_FLOAT_EQ(amb, 4.0f);
    EXPECT_FLOAT_EQ(dir, 40.0f);
}

TEST(LightingEvaluateCache, LightDirIsADerivedResidualAndMayGoNegative)
{
    // *light_dir is NOT summed independently -- lighting.c:479 computes it as
    // light_tot - *light_amb, once, at the end. The identity therefore holds
    // exactly, including when the directed term is negative. A future change
    // that clamps light_dir at zero (a plausible "fix" for negative light)
    // would break the identity here rather than silently in a render pass.
    lighting_cache_t c;
    setHalf(c.low, 0.0f, 0.0f, 0.0f, 0.0f, -100.0f, 0.0f, 5.0f);
    setHalf(c.hgh, 0.0f, 0.0f, 0.0f, 0.0f, -100.0f, 0.0f, 5.0f);
    c.max_light();
    const Ego::AxisAlignedBox3f box(Ego::Point3f(0.0f, 0.0f, 0.0f), Ego::Point3f(1.0f, 1.0f, 10.0f));

    float amb = 0.0f, dir = 0.0f;
    const float tot = lighting_cache_t::lighting_evaluate_cache(
        c, Ego::Vector3f(0.0f, 0.0f, 1.0f), 5.0f, box, &amb, &dir);

    EXPECT_FLOAT_EQ(tot, -95.0f);
    EXPECT_FLOAT_EQ(amb, 5.0f);
    EXPECT_FLOAT_EQ(dir, -100.0f);
    EXPECT_FLOAT_EQ(dir, tot - amb);
}

TEST(LightingEvaluateCache, TheTwoHalvesGateOnMaxLightIndependently)
{
    // Refreshing only the low half's _max_light leaves hgh on the ambient-only
    // short path, so half the directed light silently disappears while the
    // other half survives. Pinned because it is a realistic partial-update bug
    // shape, not a synthetic one.
    lighting_cache_t c;
    c.low._lighting[LVEC_PZ] = 40.0f;
    c.low._lighting[LVEC_AMB] = 4.0f;
    c.hgh._lighting[LVEC_PZ] = 80.0f;
    c.hgh._lighting[LVEC_AMB] = 8.0f;
    c.low.max_light();                      // hgh deliberately left stale
    ASSERT_FLOAT_EQ(c.low._max_light, 40.0f);
    ASSERT_FLOAT_EQ(c.hgh._max_light, 0.0f);

    const Ego::AxisAlignedBox3f box(Ego::Point3f(0.0f, 0.0f, 0.0f), Ego::Point3f(1.0f, 1.0f, 10.0f));
    float amb = 0.0f, dir = 0.0f;
    const float tot = lighting_cache_t::lighting_evaluate_cache(
        c, Ego::Vector3f(0.0f, 0.0f, 1.0f), 5.0f, box, &amb, &dir);

    // 0.5 * 44 (full) + 0.5 * 8 (ambient only) = 26.
    EXPECT_FLOAT_EQ(tot, 26.0f);
    EXPECT_FLOAT_EQ(amb, 6.0f);
    EXPECT_FLOAT_EQ(dir, 20.0f);
}

TEST(LightingEvaluateCache, NullOutParamsAreRedirectedToLocalsAndAreSafe)
{
    // lighting.c:452-453. Both out-params are optional.
    const lighting_cache_t c = makeHeightCache();
    const Ego::AxisAlignedBox3f box(Ego::Point3f(0.0f, 0.0f, 0.0f), Ego::Point3f(1.0f, 1.0f, 10.0f));

    const float tot = lighting_cache_t::lighting_evaluate_cache(
        c, Ego::Vector3f(0.0f, 0.0f, 1.0f), 2.5f, box, nullptr, nullptr);
    EXPECT_FLOAT_EQ(tot, 55.0f);
}

TEST(LightingEvaluateCache, DoesNotNormalizeTheNormal)
{
    const lighting_cache_t c = makeHeightCache();
    const Ego::AxisAlignedBox3f box(Ego::Point3f(0.0f, 0.0f, 0.0f), Ego::Point3f(1.0f, 1.0f, 10.0f));

    float ambUnit = 0.0f, dirUnit = 0.0f;
    const float unit = lighting_cache_t::lighting_evaluate_cache(
        c, Ego::Vector3f(0.0f, 0.0f, 1.0f), 0.0f, box, &ambUnit, &dirUnit);
    float ambTriple = 0.0f, dirTriple = 0.0f;
    const float triple = lighting_cache_t::lighting_evaluate_cache(
        c, Ego::Vector3f(0.0f, 0.0f, 3.0f), 0.0f, box, &ambTriple, &dirTriple);

    // Only the DIRECTED part scales; ambient is independent of the normal.
    EXPECT_FLOAT_EQ(dirTriple, 3.0f * dirUnit);
    EXPECT_FLOAT_EQ(ambTriple, ambUnit);
    EXPECT_FLOAT_EQ(triple, unit + 2.0f * dirUnit);
}

TEST(LightingEvaluateCache, QUIRK2_DegenerateBoxDividesByZeroButConstrainLaundersTheResult)
{
    // ***QUIRK 2.*** lighting.c:456 divides by
    // (bbox.max.z - bbox.min.z) with NO zero guard, so a zero-height box really
    // does produce NaN (z == min) or +/-Inf (z != min).
    //
    // BUT the predicted "NaN/Inf light" does NOT reach the caller. :457 pipes
    // the weight through Ego::Math::constrain, which is
    // `std::max(lower, std::min(n, upper))` (egolib/Math/Math.hpp). std::min and
    // std::max are single-`<` templates and every comparison against NaN is
    // false, so:
    //     constrain(NaN,  0, 1) == 0
    //     constrain(+Inf, 0, 1) == 1
    //     constrain(-Inf, 0, 1) == 0
    // The clamp at :457 is the ONLY thing that sanitizes the weight; by the
    // time the `if (low_wt > 0.0f)` / `if (hgh_wt > 0.0f)` guards at :466/:473
    // run, hgh_wt is already a clean 0 or 1 and low_wt = 1 - hgh_wt is finite
    // too, so those guards are not doing any NaN defence. The result is always
    // finite.
    //
    // FINDING: the real defect is silent, unsignalled BIAS -- a degenerate box
    // collapses the entire height lerp onto ONE endpoint, chosen by a
    // floating-point accident of the sign of (z - min.z), with no error path.
    // The identical unguarded division is duplicated at graphic_lighting.c:185.
    //
    // This test depends on IEEE-754 comparison semantics. -ffast-math would
    // break it for reasons unrelated to lighting.
    lighting_cache_t c;
    setHalf(c.low, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 10.0f);
    setHalf(c.hgh, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 20.0f);
    c.max_light();

    // The default-constructed AABB has min == max == (0,0,0).
    const Ego::AxisAlignedBox3f degenerate;
    ASSERT_TRUE(degenerate.is_degenerated());
    const Ego::Vector3f up(0.0f, 0.0f, 1.0f);

    float amb = 0.0f, dir = 0.0f;

    // z == min.z: 0/0 -> NaN -> hgh_wt 0 -> pure LOW.
    float tot = lighting_cache_t::lighting_evaluate_cache(c, up, 0.0f, degenerate, &amb, &dir);
    EXPECT_TRUE(std::isfinite(tot));
    EXPECT_FLOAT_EQ(tot, 10.0f);
    EXPECT_FLOAT_EQ(amb, 10.0f);

    // z > min.z: +/0 -> +Inf -> hgh_wt 1 -> pure HGH.
    tot = lighting_cache_t::lighting_evaluate_cache(c, up, 5.0f, degenerate, &amb, &dir);
    EXPECT_TRUE(std::isfinite(tot));
    EXPECT_FLOAT_EQ(tot, 20.0f);

    // z < min.z: -/0 -> -Inf -> hgh_wt 0 -> pure LOW.
    tot = lighting_cache_t::lighting_evaluate_cache(c, up, -5.0f, degenerate, &amb, &dir);
    EXPECT_TRUE(std::isfinite(tot));
    EXPECT_FLOAT_EQ(tot, 10.0f);
}

TEST(LightingEvaluateCache, QUIRK2_ConstrainLaunderingIsItselfPinned)
{
    // The behavior above rests entirely on this. Assert it directly so that a
    // change to Ego::Math::constrain fails HERE, with an obvious message,
    // instead of showing up as a mysterious lighting shift three tests up.
    const float nan = std::nanf("");
    const float inf = std::numeric_limits<float>::infinity();

    EXPECT_FLOAT_EQ(Ego::Math::constrain(nan, 0.0f, 1.0f), 0.0f);
    EXPECT_FLOAT_EQ(Ego::Math::constrain(inf, 0.0f, 1.0f), 1.0f);
    EXPECT_FLOAT_EQ(Ego::Math::constrain(-inf, 0.0f, 1.0f), 0.0f);
}

//============================================================================
// SECTION 9 -- dyna_lighting_intensity  ***QUIRK 4***
//
// `diff` is a position delta in world units (light position minus sample
// position). The falloff is the bell-shaped curve documented at
// lighting.h:145-176: f(y) = 1 - y^2*(3 - y^4)/2 with y^2 = r^2*2/765/falloff.
// The implementation at lighting.c:495 evaluates exactly that with y2 == y^2,
// so the docstring and the code agree.
//
// The expected values below are MEASURED outputs, deliberately not a
// re-derivation of the polynomial -- re-implementing the formula in the test
// would pin nothing but its own transcription.
//============================================================================

TEST(DynaLightingIntensity, InitGivesTheDocumentedDefaults)
{
    // dynalight_data_t is a plain aggregate with no constructor, so this static
    // init is the only thing that makes a light well-formed. falloff = 255 is
    // what keeps the unguarded division at lighting.c:491 safe in practice.
    dynalight_data_t d;
    dynalight_data_t::init(d);

    EXPECT_FLOAT_EQ(d.distance, 1000.0f);
    EXPECT_FLOAT_EQ(d.falloff, 255.0f);
    EXPECT_FLOAT_EQ(d.level, 0.0f);
    EXPECT_FLOAT_EQ(d.pos[kX], 0.0f);
    EXPECT_FLOAT_EQ(d.pos[kY], 0.0f);
    EXPECT_FLOAT_EQ(d.pos[kZ], 0.0f);
}

TEST(DynaLightingIntensity, FalloffCurveHitsItsMeasuredValuesAndReachesZeroAtTheCutoff)
{
    dynalight_data_t d;
    dynalight_data_t::init(d);
    d.level = 1.0f;   // falloff stays at the default 255

    // Cutoff radius is where y2 == 1, i.e. r = sqrt(765*falloff/2) = 312.310...
    EXPECT_FLOAT_EQ(dyna_lighting_intensity(&d, Ego::Vector3f(0.0f, 0.0f, 0.0f)), 1.0f);
    EXPECT_NEAR(dyna_lighting_intensity(&d, Ego::Vector3f(100.0f, 0.0f, 0.0f)), 0.846752f, 1.0e-5f);
    EXPECT_NEAR(dyna_lighting_intensity(&d, Ego::Vector3f(200.0f, 0.0f, 0.0f)), 0.419337f, 1.0e-5f);

    // Just inside the cutoff: small and strictly positive. Pinned as a range
    // rather than a literal -- this is a catastrophic-cancellation region.
    const float nearCutoff = dyna_lighting_intensity(&d, Ego::Vector3f(312.0f, 0.0f, 0.0f));
    EXPECT_GT(nearCutoff, 0.0f);
    EXPECT_LT(nearCutoff, 1.0e-4f);

    // Past the cutoff: exactly zero.
    EXPECT_FLOAT_EQ(dyna_lighting_intensity(&d, Ego::Vector3f(312.4f, 0.0f, 0.0f)), 0.0f);
    EXPECT_FLOAT_EQ(dyna_lighting_intensity(&d, Ego::Vector3f(400.0f, 0.0f, 0.0f)), 0.0f);

    // Strictly decreasing across the whole supported range. Catches a wholesale
    // substitution of the falloff curve that the fixed points above might miss.
    float previous = 2.0f;
    for (float r = 0.0f; r <= 312.0f; r += 8.0f)
    {
        const float here = dyna_lighting_intensity(&d, Ego::Vector3f(r, 0.0f, 0.0f));
        EXPECT_LT(here, previous) << "r=" << r;
        previous = here;
    }
}

TEST(DynaLightingIntensity, IsRadiallySymmetricInXY)
{
    dynalight_data_t d;
    dynalight_data_t::init(d);
    d.level = 1.0f;

    const float alongX = dyna_lighting_intensity(&d, Ego::Vector3f(100.0f, 0.0f, 0.0f));
    const float alongY = dyna_lighting_intensity(&d, Ego::Vector3f(0.0f, 100.0f, 0.0f));
    const float diagonal = dyna_lighting_intensity(&d, Ego::Vector3f(70.710678f, 70.710678f, 0.0f));

    EXPECT_FLOAT_EQ(alongY, alongX);
    EXPECT_NEAR(diagonal, alongX, 1.0e-5f);
}

TEST(DynaLightingIntensity, FINDING_FalloffIsCylindricalBecauseTheZDeltaIsIgnored)
{
    // FINDING: lighting.c:490 computes rho_sqr from diff[kX] and diff[kY] only.
    // diff[kZ] is never read, so the falloff volume is an infinite vertical
    // cylinder rather than a sphere: a dynamic light one million world units
    // overhead is exactly as bright as one at the sample point.
    //
    // Consequence in the caller: graphic_lighting_dynalist.c builds `nrm` with
    // Z = pdyna->pos[kZ] - tmem._bbox.get_min()[ZZ] for the `low` half and
    // Z = pdyna->pos[kZ] - tmem._bbox.get_max()[ZZ] for the `hgh` half, and
    // calls sum_dyna_lighting once with each. Since only X and Y are read, both
    // calls compute the identical intensity -- the low/hgh height split is
    // currently a NO-OP for dynamic lights. Reported, not fixed.
    dynalight_data_t d;
    dynalight_data_t::init(d);
    d.level = 1.0f;

    const float atOrigin = dyna_lighting_intensity(&d, Ego::Vector3f(0.0f, 0.0f, 0.0f));
    const float wayAbove = dyna_lighting_intensity(&d, Ego::Vector3f(0.0f, 0.0f, 1.0e6f));
    EXPECT_FLOAT_EQ(atOrigin, 1.0f);
    EXPECT_FLOAT_EQ(wayAbove, 1.0f);

    // And the Z delta does not perturb an off-centre sample either.
    const float flat = dyna_lighting_intensity(&d, Ego::Vector3f(100.0f, 0.0f, 0.0f));
    const float raised = dyna_lighting_intensity(&d, Ego::Vector3f(100.0f, 0.0f, 500.0f));
    EXPECT_FLOAT_EQ(raised, flat);
}

TEST(DynaLightingIntensity, QUIRK4_ReturnFalseInAFloatFunctionIsObservablyZeroAndTheCurveIsContinuous)
{
    // ***QUIRK 4.*** lighting.c:493 is `if (y2 > 1.0f) return false;` inside a
    // function whose return type is float. `false` converts to 0.0f, so the
    // behavior is correct by accident and matches the explicit `return 0.0f` at
    // :488. It is a type/readability defect, not a behavioral one, and it does
    // not warn under -Wall -Wextra because the conversion is legal.
    // FINDING -- reported, not fixed.
    //
    // WHAT THIS TEST CAN AND CANNOT SEE. It pins the observable zero and the
    // continuity of the curve across the cutoff. It does NOT and CANNOT pin
    // the strict `>`: at exactly y2 == 1 the polynomial
    // 1 - 0.5*1*(3 - 1) evaluates to exactly 0.0f, which is bit-identical to
    // the value `false` converts to, so `>` and `>=` are indistinguishable
    // through this API at every input. Do not add a comment claiming
    // otherwise. What IS pinned against mutation is the cutoff constant
    // itself, by the measured falloff curve in the sibling test above.
    // Verified by mutation: rewriting :493 as `return 0.0f` leaves this file
    // green, which is the evidence for calling the defect behaviour-neutral.
    dynalight_data_t d;
    dynalight_data_t::init(d);
    d.level = 1.0f;
    d.falloff = 1.0f;   // cutoff at rho_sqr == 765/2 == 382.5

    // JUST INSIDE the cutoff: the polynomial path really does run, and it
    // approaches zero smoothly rather than dropping off a cliff.
    const float justInside = dyna_lighting_intensity(&d, Ego::Vector3f(0.0f, std::sqrt(380.0f), 0.0f));
    EXPECT_GT(justInside, 0.0f);
    EXPECT_LT(justInside, 1.0e-3f);

    // EXACTLY at the cutoff. Getting y2 == 1.0f bit-exactly matters, and
    // sqrt(382.5f) squared is 382.500031f -- y2 = 1.00000012f, which is
    // strictly GREATER than 1 and takes the guard, not the polynomial. So
    // build the falloff from the radial distance instead, with the same
    // operation order lighting.c:491 uses; then y2 is x/x == 1.0f exactly and
    // the polynomial branch is the one that runs.
    const float rhoSqr = 16.0f * 16.0f;              // diff = (16, 0, 0)
    d.falloff = rhoSqr * 2.0f / 765.0f;              // makes y2 == 1.0f exactly
    const float atBoundary = dyna_lighting_intensity(&d, Ego::Vector3f(16.0f, 0.0f, 0.0f));
    EXPECT_FLOAT_EQ(atBoundary, 0.0f);   // exactly 0, from the polynomial

    // PAST the cutoff: rho_sqr = 400 against falloff 1 -> y2 > 1 -> the
    // `return false` path. Same observable value, different branch.
    d.falloff = 1.0f;
    const float pastBoundary = dyna_lighting_intensity(&d, Ego::Vector3f(20.0f, 0.0f, 0.0f));
    EXPECT_FLOAT_EQ(pastBoundary, 0.0f);
}

TEST(DynaLightingIntensity, RejectsNullAndZeroLevelAndPassesNegativeLevelThrough)
{
    // lighting.c:488 -- both rejections return an explicit 0.0f.
    EXPECT_FLOAT_EQ(dyna_lighting_intensity(nullptr, Ego::Vector3f(0.0f, 0.0f, 0.0f)), 0.0f);

    dynalight_data_t d;
    dynalight_data_t::init(d);
    ASSERT_FLOAT_EQ(d.level, 0.0f);
    EXPECT_FLOAT_EQ(dyna_lighting_intensity(&d, Ego::Vector3f(0.0f, 0.0f, 0.0f)), 0.0f);

    // A negative level is NOT clamped; it produces negative intensity. The one
    // production caller that cares wraps the call in std::abs
    // (graphic_lighting_dynalist.c:233).
    d.level = -1.0f;
    EXPECT_FLOAT_EQ(dyna_lighting_intensity(&d, Ego::Vector3f(0.0f, 0.0f, 0.0f)), -1.0f);
}

TEST(DynaLightingIntensity, FINDING_ZeroFalloffDividesByZeroAndTheNaNEscapesTheRangeGuard)
{
    // FINDING: lighting.c:491 divides by pdyna->falloff with no guard.
    //   falloff == 0 and rho_sqr == 0  ->  0/0  ->  y2 is NaN. The range check
    //   at :493 is `y2 > 1.0f`, which is FALSE for NaN, so the guard does not
    //   fire and NaN flows through the polynomial and out of the function.
    //   sum_dyna_lighting then fails its `0.0f == level` test at
    //   lighting.c:507 (every comparison against NaN is false) and accumulates
    //   NaN straight into LVEC_AMB at :104 -- asserted below rather than
    //   merely claimed, so the propagation is pinned and not argued.
    //   falloff == 0 and rho_sqr > 0   ->  +Inf, which DOES trip the guard -> 0.
    //
    // Reachability, stated precisely rather than dramatically:
    //   - dynalight_data_t::init sets falloff = 255.
    //   - The Gouraud mesh path filters the light out first --
    //     graphic_lighting_dynalist.c:190 skips any light with
    //     `falloff <= 0.0f`, so sum_dyna_lighting is not reached with a zero
    //     falloff there.
    //   - The non-Gouraud path at graphic_lighting_dynalist.c:233 calls
    //     dyna_lighting_intensity with NO such guard. A NaN there makes
    //     dyna_weight_sum NaN, and the `dyna_weight_sum > 0.0f` test at :244
    //     is false for NaN, so the combined "fake" dynalight is silently
    //     dropped for the frame rather than poisoning anything.
    //   - The value itself is unvalidated on the way in
    //     (ParticleProfile.cpp:234 reads it straight from the profile file).
    // So this is pinned as an API-level property of lighting.c, not as a
    // shipped rendering bug. Reported, not fixed.
    dynalight_data_t d;
    dynalight_data_t::init(d);
    d.level = 1.0f;
    d.falloff = 0.0f;

    const float atOrigin = dyna_lighting_intensity(&d, Ego::Vector3f(0.0f, 0.0f, 0.0f));
    EXPECT_TRUE(std::isnan(atOrigin));   // sign of the NaN is not pinned

    const float offOrigin = dyna_lighting_intensity(&d, Ego::Vector3f(2.0f, 0.0f, 0.0f));
    EXPECT_FLOAT_EQ(offOrigin, 0.0f);

    // The NaN escapes into the cache, and only into the ambient slot.
    LightingVector lvec{};
    lvec.fill(0.0f);
    const bool applied = sum_dyna_lighting(&d, lvec, idlib::zero<Ego::Vector3f>());
    EXPECT_TRUE(applied);
    EXPECT_TRUE(std::isnan(lvec[LVEC_AMB]));
    for (size_t i = 0; i < static_cast<size_t>(LVEC_AMB); ++i)
    {
        EXPECT_FLOAT_EQ(lvec[i], 0.0f) << "slot " << i;
    }
}

TEST(DynaLightingIntensity, FINDING_NegativeFalloffInvertsTheCurveSoLightGrowsWithDistance)
{
    // FINDING: a negative falloff makes y2 negative, so `y2 > 1.0f` never fires
    // and the polynomial 1 - 0.5*y2*(3 - y2*y2) rises above 1 without bound as
    // the sample moves away. Unguarded, same root cause as the zero case above.
    dynalight_data_t d;
    dynalight_data_t::init(d);
    d.level = 1.0f;
    d.falloff = -255.0f;

    // `nearSample` / `farSample`, not `near` / `far`: both of those are
    // object-like macros in the mingw-w64 windows headers, and AGENTS.md keeps
    // native Windows a first-class target.
    const float nearSample = dyna_lighting_intensity(&d, Ego::Vector3f(100.0f, 0.0f, 0.0f));
    const float farSample = dyna_lighting_intensity(&d, Ego::Vector3f(200.0f, 0.0f, 0.0f));
    EXPECT_GT(nearSample, 1.0f);
    EXPECT_GT(farSample, nearSample);
}

//============================================================================
// SECTION 10 -- sum_dyna_lighting  ***QUIRK 3***
//============================================================================

TEST(SumDynaLighting, QUIRK3_ContributionIsPurelyAmbientAndNeverDirectional)
{
    // ***QUIRK 3 -- the load-bearing tripwire in this file.***
    //
    // sum_dyna_lighting names its third parameter `nrm`, but it is a POSITION
    // DELTA, not a normal: lighting.c:506 forwards it as the `diff` argument of
    // dyna_lighting_intensity, and the sole caller
    // (graphic_lighting_dynalist.c) builds it as pdyna->pos minus the grid
    // point. Having computed the intensity, :513 calls
    //     lighting_vector_sum(lighting, idlib::zero<Ego::Vector3f>(), 0.0f, level)
    // -- a ZERO direction vector with direct == 0 and ambient == level. Only
    // `lighting[LVEC_AMB] += level` executes. All six directional slots are
    // untouched. (The zero vector is doubly redundant: every directional add is
    // multiplied by direct == 0, and a zero component takes no branch anyway.)
    //
    // This is DELIBERATE, and the in-code comment at lighting.c:509-512 says
    // so. Git backs it up: commit 31f4fd1a2b5c3a3b2f423e760d5dc7229a15ab6b
    // ("Update lighting logic to restore ambient contribution from dynalights",
    // 2026-04-16) replaced a purely directional
    // `lighting_vector_sum(lighting, local_nrm, level, 0.0f)` -- with an
    // explicit normalization of the delta -- with the ambient form above, to
    // keep handheld torches usable in zero-ambient modules such as palshad.mod.
    //
    // THIS TEST IS THE TRIPWIRE. Any renderer change that "restores directional
    // dynalights" reverts that fix and re-breaks palshad.mod. It must fail
    // loudly here first.
    dynalight_data_t d;
    dynalight_data_t::init(d);
    d.level = 1.0f;
    d.falloff = 1.0f;

    LightingVector lvec{};
    lvec.fill(0.0f);

    const bool ok = sum_dyna_lighting(&d, lvec, Ego::Vector3f(1.0f, 0.0f, 0.0f));
    EXPECT_TRUE(ok);

    // Every directional slot is EXACTLY zero.
    EXPECT_FLOAT_EQ(lvec[LVEC_PX], 0.0f);
    EXPECT_FLOAT_EQ(lvec[LVEC_MX], 0.0f);
    EXPECT_FLOAT_EQ(lvec[LVEC_PY], 0.0f);
    EXPECT_FLOAT_EQ(lvec[LVEC_MY], 0.0f);
    EXPECT_FLOAT_EQ(lvec[LVEC_PZ], 0.0f);
    EXPECT_FLOAT_EQ(lvec[LVEC_MZ], 0.0f);
    // ...and only ambient moved. 255 * intensity(rho_sqr = 1, falloff = 1).
    EXPECT_NEAR(lvec[LVEC_AMB], 254.0f, 1.0e-2f);
    EXPECT_GT(lvec[LVEC_AMB], 0.0f);
}

TEST(SumDynaLighting, QUIRK3_AccumulatesAcrossCallsAndScalesTheIntensityBy255)
{
    dynalight_data_t d;
    dynalight_data_t::init(d);
    d.level = 1.0f;

    LightingVector lvec{};
    lvec.fill(0.0f);

    // rho_sqr == 0 -> intensity is exactly 1 -> lighting.c:506 contributes
    // exactly 255. Two calls accumulate to 510, pinning both the 255 scale
    // factor and the accumulate (not assign) semantics inherited from
    // lighting_vector_sum.
    EXPECT_TRUE(sum_dyna_lighting(&d, lvec, idlib::zero<Ego::Vector3f>()));
    EXPECT_FLOAT_EQ(lvec[LVEC_AMB], 255.0f);
    EXPECT_TRUE(sum_dyna_lighting(&d, lvec, idlib::zero<Ego::Vector3f>()));
    EXPECT_FLOAT_EQ(lvec[LVEC_AMB], 510.0f);
}

TEST(SumDynaLighting, QUIRK3_TheZComponentOfTheDeltaIsIgnoredSoDistanceCanBeMisleading)
{
    // Direct consequence of the cylindrical falloff: a light 99 units straight
    // up contributes MORE than one a single unit sideways, because only the XY
    // radial distance counts. This is the caller-visible face of the low/hgh
    // no-op described in DynaLightingIntensity.FINDING_FalloffIsCylindrical...
    dynalight_data_t d;
    dynalight_data_t::init(d);
    d.level = 1.0f;
    d.falloff = 1.0f;

    LightingVector sideways{}, overhead{};
    sideways.fill(0.0f);
    overhead.fill(0.0f);

    EXPECT_TRUE(sum_dyna_lighting(&d, sideways, Ego::Vector3f(1.0f, 0.0f, 0.0f)));
    EXPECT_TRUE(sum_dyna_lighting(&d, overhead, Ego::Vector3f(0.0f, 0.0f, 99.0f)));

    EXPECT_FLOAT_EQ(overhead[LVEC_AMB], 255.0f);         // full strength
    EXPECT_LT(sideways[LVEC_AMB], overhead[LVEC_AMB]);   // the *closer* light is dimmer
}

TEST(SumDynaLighting, ReturnValueCannotDistinguishAppliedFromOutOfRange)
{
    // FINDING (API-level): the bool is false ONLY for a null light
    // (lighting.c:504). The out-of-range early return at :507 returns TRUE with
    // the lighting vector untouched, so a caller cannot use the return value to
    // learn whether any light was actually applied. Both production call sites
    // discard it.
    dynalight_data_t d;
    dynalight_data_t::init(d);
    d.level = 1.0f;
    d.falloff = 1.0f;

    LightingVector lvec{};
    lvec.fill(0.0f);

    // Out of range (rho_sqr = 400 > 382.5) -> intensity 0 -> level 0.
    const bool outOfRange = sum_dyna_lighting(&d, lvec, Ego::Vector3f(20.0f, 0.0f, 0.0f));
    EXPECT_TRUE(outOfRange);
    expectVector(lvec, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    // A zero-level light also short-circuits at :507 and still returns true.
    d.level = 0.0f;
    EXPECT_TRUE(sum_dyna_lighting(&d, lvec, idlib::zero<Ego::Vector3f>()));
    expectVector(lvec, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    // Only a null light returns false.
    lvec[LVEC_AMB] = 12.0f;
    const bool nullLight = sum_dyna_lighting(nullptr, lvec, idlib::zero<Ego::Vector3f>());
    EXPECT_FALSE(nullLight);
    EXPECT_FLOAT_EQ(lvec[LVEC_AMB], 12.0f);   // untouched
}
