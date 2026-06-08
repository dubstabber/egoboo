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

/// @file egolib/Log/_Include.cpp
/// @details Basic logging system

#include "egolib/Log/_Include.hpp"

#include "egolib/Log/DefaultTarget.hpp"
#include "egolib/Log/ConsoleColor.hpp"

namespace Log {

/**
 * @brief
 *  The single target of this log system.
 */
static std::unique_ptr<Log::Target> g_target = nullptr;
static bool _atexit_registered = false;

/**
 * @brief
 *  The engine-published active-target override, if installed.
 * @remark
 *  Relocated from the engine context so that the Log subsystem owns active-target
 *  resolution and no longer depends upward on the engine layer.
 */
static Log::Target* g_activeTarget = nullptr;

void initialize(const std::string& filename, Log::Level level) {
	if (!g_target) {
		g_target = std::make_unique<DefaultTarget>(filename, level);
	}
	if (!_atexit_registered) {
		if (atexit(Log::uninitialize)) {
			g_target = nullptr;
			throw std::runtime_error("unable to initialize logging system");
		}
		_atexit_registered = true;
	}
}

void uninitialize() {
	if (g_target) {
		g_target = nullptr;
	}
}

Target& get() {
	if (!g_target) {
		throw std::logic_error("logging system is not initialized");
	}
	return *g_target;
}

Target* tryActiveTarget() {
	if (g_activeTarget) {
		return g_activeTarget;
	}
	return g_target.get();
}

void installActiveTarget(Target& target) {
	if (g_activeTarget) {
		throw std::logic_error("log target already installed");
	}
	g_activeTarget = &target;
}

void clearActiveTarget() {
	g_activeTarget = nullptr;
}

Target* tryInstalledTarget() {
	return g_activeTarget;
}

Target& activeTarget() {
	if (auto* logTarget = tryActiveTarget()) {
		return *logTarget;
	}
	return get();
}

} // namespace Log
