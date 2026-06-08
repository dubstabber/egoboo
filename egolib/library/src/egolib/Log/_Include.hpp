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

/// @file egolib/Log/_Include.hpp
/// @details Basic logging system

#pragma once

#define EGOLIB_LOG_PRIVATE 1
#include "egolib/Log/Entry.hpp"
#include "egolib/Log/Target.hpp"
#include "egolib/Log/Level.hpp"

namespace Log {

	/**
	 * @brief
	 *  Initialize the logging system.
	 * @param filename
	 *  the file name
	 * @param level
	 *  the level
	 * @return
	 *  Returns if the logging system is already initialized.
	 * @throw std::runtime_error
	 *  if initialization fails
	 */
	void initialize(const std::string& filename, Level level);

	/**
	 * @brief
	 *  Uninitialize the logging system
	 * @remark
	 *  Returns if the logging system is not initialized.
	 */
	void uninitialize();

	/**
	 * @brief
	 *  Get the default target.
	 * @return
	 *  the default target
	 * @throw std::logic_error
	 *  if the logging system is not initialized
	 */
	Target& get();

	/**
	 * @brief
	 *  Try to get the active log target.
	 * @return
	 *  the engine-published log target when installed, otherwise the default target,
	 *  or @a nullptr when no target is available
	 */
	Target* tryActiveTarget();

	/**
	 * @brief
	 *  Get the active log target.
	 * @return
	 *  the engine-published log target when installed, otherwise the default target
	 * @throw std::logic_error
	 *  if no log target is available
	 */
	Target& activeTarget();

	/**
	 * @brief
	 *  Install the engine-published active log-target override.
	 * @param target
	 *  the target to install
	 * @throw std::logic_error
	 *  if an active target is already installed
	 * @remark
	 *  Ownership of the engine-installed override lives in the Log subsystem so that
	 *  lower layers can resolve the active target without depending on the engine layer.
	 */
	void installActiveTarget(Target& target);

	/**
	 * @brief
	 *  Clear the engine-published active log-target override (reverting to the default target).
	 */
	void clearActiveTarget();

	/**
	 * @brief
	 *  Get the engine-published active log-target override, if one is installed.
	 * @return
	 *  the installed override target, or @a nullptr when none is installed
	 * @remark
	 *  Unlike #tryActiveTarget(), this does not fall back to the default target; it
	 *  reports only whether an engine override is currently installed.
	 */
	Target* tryInstalledTarget();

} // namespace Log
