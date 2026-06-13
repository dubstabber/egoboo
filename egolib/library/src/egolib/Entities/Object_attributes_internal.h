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

/// @file egolib/Entities/Object_attributes_internal.h
/// @brief Private shared helper for the Object_attributes TU family.
/// @details This header is NOT part of the public API. It is included only by
///          Object_attributes.cpp and Object_attributes_behaviors.cpp.
///          heldItem() is the lone former-anonymous-namespace helper that crosses the cut
///          (getAttribute in the residual; polymorphObject/setTeam in the behaviors TU), so it
///          is promoted from file-scope internal linkage to an inline function in namespace
///          object_attributes_detail + a global using-directive — vague COMDAT linkage,
///          -Wunused-function-clean, and a distinct mangled name from the other anon-namespace
///          heldItem helpers elsewhere in egolib-library. Mirrors the object_detail idiom in
///          Object_internal.h.
#pragma once

#include "egolib/Entities/Object_internal.h"  // Object (full), GameSessionContext, ObjectHandler, slot_t

namespace object_attributes_detail {

inline const std::shared_ptr<Object>& heldItem(const Object& object, slot_t slot)
{
    return GameSessionContext::get().activeModule().getObjectHandler()[object.getHeldObject(slot)];
}

}

using namespace object_attributes_detail;
