#pragma once

#include "egolib/typedef.h"  // PRO_REF

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

    /**
    * @brief Gets a shared_ptr to the current ObjectProfile associated with this character.
    *        The ObjectProfile can change for polymorphing objects.
    **/
    virtual const std::shared_ptr<ObjectProfile>& getProfile() const = 0;
    virtual PRO_REF getProfileRef() const = 0;
};
