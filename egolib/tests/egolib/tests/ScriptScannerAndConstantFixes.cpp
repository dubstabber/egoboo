#include "gtest/gtest.h"

#include "egolib/Log/Target.hpp"
#include "egolib/Script/Constant.hpp"
#include "egolib/Script/Scanner.hpp"
#include "egolib/Script/script.h"
#include "egolib/game/Core/EngineContext.hpp"

#include <vector>

// Characterizes three Pass-340 latent-bug fixes:
//
// - Ego::Script::Constant::operator== used to fall off the end of a non-void function
//   for a corrupted/out-of-range Kind (undefined behavior). The switch already covers
//   every declared Kind value, so the bug was never reachable through legitimate
//   construction; this file pins the still-correct behavior for every legitimate Kind
//   combination as a regression guard.
// - Ego::Script::Scanner<...>::ERROR() used to route the extended-symbol error sentinel
//   (Traits<char>::error(), an ExtendedSymbolType/int) through a char-typed helper,
//   silently truncating it (observed: 15630467 -> -125). This meant ERROR() could never
//   match a genuine decode-error symbol, and could instead spuriously match an ordinary
//   input value that happened to equal the truncated byte. This file pins that ERROR()
//   now matches the untruncated sentinel and does not match the previously-truncated value.
// - script_state_t::loadVariable() had a switch with a default case that unconditionally
//   throws (via onVariableNotDefinedError), so it was never actually possible to fall off
//   the end at runtime; the compiler could not see that because onVariableNotDefinedError
//   was not marked [[noreturn]]. While auditing this, VARSELFWIS and VARTARGETWIS turned
//   out to be genuinely unhandled by loadVariable's switch (present in Variables.in/the
//   ScriptVariables enum, but with no corresponding DEFINE(...) case here) - i.e. this
//   default path is reachable through ordinary script content that reads "selfwis" or
//   "targetwis". This file pins that loading either of those variables throws
//   idlib::runtime_error (unchanged observable behavior; [[noreturn]] only removes the
//   compiler's false "falls off the end" warning).

namespace
{

using Ego::Script::Constant;

TEST(ConstantEquality, VoidConstantsAreEqual)
{
    Constant a;
    Constant b;
    EXPECT_TRUE(a == b);
}

TEST(ConstantEquality, IntegerConstantsCompareByValue)
{
    Constant a(1);
    Constant b(1);
    Constant c(2);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
}

TEST(ConstantEquality, StringConstantsCompareByValue)
{
    Constant a(std::string("foo"));
    Constant b(std::string("foo"));
    Constant c(std::string("bar"));
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
}

TEST(ConstantEquality, DifferentKindsAreNeverEqual)
{
    Constant voidConstant;
    Constant integerConstant(0);
    Constant stringConstant(std::string(""));
    EXPECT_FALSE(voidConstant == integerConstant);
    EXPECT_FALSE(integerConstant == stringConstant);
    EXPECT_FALSE(stringConstant == voidConstant);
}

using Traits = Ego::Script::Traits<char>;
using ExtendedSymbolType = Traits::ExtendedType;
using Scanner = Ego::Script::Scanner<Traits>;

TEST(ScannerErrorSentinel, MatchesTheFullWidthErrorSentinel)
{
    auto errorExpr = Scanner::ERROR();

    std::vector<ExtendedSymbolType> sequence{Traits::error()};
    EXPECT_TRUE(static_cast<bool>(errorExpr(sequence.begin(), sequence.end())));
}

TEST(ScannerErrorSentinel, DoesNotMatchThePreviouslyTruncatedByteValue)
{
    auto errorExpr = Scanner::ERROR();

    // Before the fix, Traits::error() (0xee8083 == 15630467) was narrowed to a char
    // parameter, truncating it to -125. Any ordinary input symbol equal to -125 would
    // then have spuriously matched ERROR(); confirm that no longer happens.
    std::vector<ExtendedSymbolType> sequence{-125};
    EXPECT_FALSE(static_cast<bool>(errorExpr(sequence.begin(), sequence.end())));
}

class StubLogTarget : public Log::Target
{
public:
    using Log::Target::Target;

protected:
    void writev(Log::Level, const char*, va_list) override {}
};

class LoadVariableFixture : public ::testing::Test
{
protected:
    void SetUp() override
    {
        auto& context = EngineContext::get();
        if (context.tryLogTarget())
        {
            context.clearLogTarget();
        }
        context.installLogTarget(logTarget);
    }

    void TearDown() override
    {
        auto& context = EngineContext::get();
        if (context.tryLogTarget())
        {
            context.clearLogTarget();
        }
    }

    StubLogTarget logTarget;
};

TEST_F(LoadVariableFixture, LoadingSelfWisdomThrowsBecauseItIsUnhandled)
{
    script_state_t scriptState{};
    ai_state_t aiState{};
    const Ego::Script::ScriptOperandContext context{};

    EXPECT_THROW(scriptState.loadVariable(static_cast<uint8_t>(Ego::Script::VARSELFWIS), aiState, context),
                 idlib::runtime_error);
}

TEST_F(LoadVariableFixture, LoadingTargetWisdomThrowsBecauseItIsUnhandled)
{
    script_state_t scriptState{};
    ai_state_t aiState{};
    const Ego::Script::ScriptOperandContext context{};

    EXPECT_THROW(scriptState.loadVariable(static_cast<uint8_t>(Ego::Script::VARTARGETWIS), aiState, context),
                 idlib::runtime_error);
}

} // namespace
