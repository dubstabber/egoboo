#pragma once

#include <memory>

class ObjectProfile;

/// @brief Role interface exposing an object's static profile (template) data,
///        decoupling profile-only consumers from the full Object class. Lets a
///        function that needs nothing from an object but its profile take a
///        `const IProfiled&` instead of the whole Object.
class IProfiled
{
public:
    virtual ~IProfiled() = default;

    /// @brief The object's profile (its static, per-type data).
    virtual const std::shared_ptr<ObjectProfile>& getProfile() const = 0;
};
