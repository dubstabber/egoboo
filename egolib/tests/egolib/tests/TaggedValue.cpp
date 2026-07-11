#include "gtest/gtest.h"

#include <sstream>
#include <type_traits>

#include "egolib/Script/Interpreter/InvalidCastException.hpp"
#include "egolib/Script/Interpreter/Tag.hpp"
#include "egolib/Script/Interpreter/TaggedValue.hpp"

namespace
{
using Ego::Script::Interpreter::IntegerValue;
using Ego::Script::Interpreter::InvalidCastException;
using Ego::Script::Interpreter::ObjectValue;
using Ego::Script::Interpreter::Tag;
using Ego::Script::Interpreter::TaggedValue;

static_assert(std::is_same<ObjectValue, ObjectRef>::value,
              "VM object values must remain stable non-owning object references");

TEST(TaggedValue, ObjectRefConstructionCopyAndAssignmentPreserveTheHandle)
{
    const ObjectRef ref(42);
    const TaggedValue value(ref);

    EXPECT_EQ(value.getTag(), Tag::Object);
    EXPECT_EQ(static_cast<ObjectValue>(value), ref);

    const TaggedValue copied(value);
    EXPECT_EQ(copied.getTag(), Tag::Object);
    EXPECT_EQ(static_cast<ObjectValue>(copied), ref);

    TaggedValue copyAssigned(IntegerValue(7));
    copyAssigned = value;
    EXPECT_EQ(copyAssigned.getTag(), Tag::Object);
    EXPECT_EQ(static_cast<ObjectValue>(copyAssigned), ref);

    copyAssigned = copyAssigned;
    EXPECT_EQ(static_cast<ObjectValue>(copyAssigned), ref);

    copyAssigned = ObjectRef::Invalid;
    EXPECT_EQ(copyAssigned.getTag(), Tag::Object);
    EXPECT_EQ(static_cast<ObjectValue>(copyAssigned), ObjectRef::Invalid);
}

TEST(TaggedValue, ObjectRefConversionRejectsOtherTags)
{
    const TaggedValue integer(IntegerValue(7));
    EXPECT_THROW(static_cast<ObjectValue>(integer), InvalidCastException);
}

TEST(TaggedValue, ObjectRefStreamingPrintsValidAndInvalidHandleValues)
{
    const ObjectRef validRef(42);
    std::ostringstream valid;
    valid << TaggedValue(validRef);
    EXPECT_EQ(valid.str(), "Object" + std::to_string(validRef.get()));

    std::ostringstream invalid;
    invalid << TaggedValue(ObjectRef::Invalid);
    EXPECT_EQ(invalid.str(), "Object" + std::to_string(ObjectRef::Invalid.get()));
}
} // namespace
