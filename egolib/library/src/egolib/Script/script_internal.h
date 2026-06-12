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

/// @file  egolib/Script/script_internal.h
/// @brief Private seam between the VM driver (script.c) and the operand evaluator
///        (script_operand.c): extern declarations for the two file-scope globals that
///        updateScriptErrorContext (script.c) writes and run_operand (script_operand.c) reads.

#pragma once

#include "egolib/typedef.h"  // ObjectProfileRef

/// @brief The ObjectProfile reference of the object whose script caused an error.
/// Set by updateScriptErrorContext (script.c); read by run_operand (script_operand.c)
/// and dumpDebugScriptState (script.c).
extern ObjectProfileRef script_error_model;

/// @brief The class name of the object whose script caused an error.
/// Set by updateScriptErrorContext (script.c); read by run_operand (script_operand.c)
/// and dumpDebugScriptState (script.c).
extern const char *script_error_classname;
