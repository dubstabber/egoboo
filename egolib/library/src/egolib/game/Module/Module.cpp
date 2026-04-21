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

/// @file egolib/game/Module/Module.cpp
/// @brief GameModule accessors and lightweight queries.

#include "egolib/game/Module/Module_internal.h"

namespace
{
ego_mesh_t& requireMesh(const std::shared_ptr<ego_mesh_t>& mesh)
{
    if (!mesh)
    {
        throw idlib::argument_null_error(__FILE__, __LINE__, "mesh");
    }

    return *mesh;
}
}

int GameModule::getPassageCount()
{
    return _passages.size();
}

std::shared_ptr<Passage> GameModule::getPassageByID(int id)
{
    if (id < 0 || id >= _passages.size())
    {
        return nullptr;
    }

    return _passages[id];
}

uint8_t GameModule::getImportAmount() const
{
    return _moduleProfile->getImportAmount();
}

uint8_t GameModule::getPlayerAmount() const
{
    return _moduleProfile->getMaxPlayers();
}

bool GameModule::isImportValid() const
{
    return _moduleProfile->getImportAmount() > 0;
}

const std::string& GameModule::getPath() const
{
    return _moduleProfile->getFolderName();
}

bool GameModule::canRespawnAnyTime() const
{
    return _moduleProfile->hasRespawnAnytime();
}

uint8_t GameModule::getMaxPlayers() const
{
    return _moduleProfile->getMaxPlayers();
}

uint8_t GameModule::getMinPlayers() const
{
    return _moduleProfile->getMinPlayers();
}

bool GameModule::isInside(const float x, const float y) const
{
    return x >= 0 && x < _mesh->_tmem._edge_x && y >= 0 && y < _mesh->_tmem._edge_y;
}

bool GameModule::isInsidePitBounds(float x, float y) const
{
    const ego_mesh_t& mesh = requireMesh(_mesh);
    return x > EDGE && y > EDGE &&
           x < mesh._tmem._edge_x - EDGE &&
           y < mesh._tmem._edge_y - EDGE;
}

bool GameModule::setTileType(Index1D tileIndex, uint16_t tileType)
{
    ego_mesh_t& mesh = requireMesh(_mesh);
    return mesh.set_texture(tileIndex, tileType);
}

bool GameModule::tryGetTileTypeAtPosition(const Ego::Vector2f& position, uint16_t& tileType) const
{
    const ego_mesh_t& mesh = requireMesh(_mesh);
    const Index1D tileIndex = mesh.getTileIndex(position);
    if (Index1D::Invalid == tileIndex)
    {
        return false;
    }

    tileType = mesh.getTileInfo(tileIndex)._img & TILE_LOWER_MASK;
    return true;
}

bool GameModule::setTileTypeAtPosition(const Ego::Vector2f& position, uint16_t tileType)
{
    ego_mesh_t& mesh = requireMesh(_mesh);
    const Index1D tileIndex = mesh.getTileIndex(position);
    return mesh.set_texture(tileIndex, tileType);
}

ObjectRef GameModule::getTeamLeaderRef(TEAM_REF teamRef) const
{
    if (!VALID_TEAM_RANGE(teamRef))
    {
        return ObjectRef::Invalid;
    }

    return _teamList[teamRef].getLeaderRef();
}

ObjectRef GameModule::getTeamCallerForHelpRef(TEAM_REF teamRef) const
{
    if (!VALID_TEAM_RANGE(teamRef))
    {
        return ObjectRef::Invalid;
    }

    return _teamList[teamRef].getSissyRef();
}

uint16_t GameModule::getTeamMorale(TEAM_REF teamRef) const
{
    if (!VALID_TEAM_RANGE(teamRef))
    {
        return 0;
    }

    return _teamList[teamRef].getMorale();
}

void GameModule::giveTeamExperience(TEAM_REF teamRef, int amount, XPType type) const
{
    if (!VALID_TEAM_RANGE(teamRef))
    {
        return;
    }

    _teamList[teamRef].giveTeamExperience(amount, type);
}

std::shared_ptr<const Ego::Texture> GameModule::getTileTexture(const size_t index)
{
    if (index >= _tileTextures.size()) return nullptr;
    return _tileTextures[index].get_ptr();
}

std::shared_ptr<const Ego::Texture> GameModule::getWaterTexture(const uint8_t layer)
{
    if (layer > _waterTextures.size()) return nullptr;
    return _waterTextures[layer].get_ptr();
}

water_instance_t& GameModule::getWater()
{
    return _water;
}

std::shared_ptr<Ego::Player>& GameModule::getPlayer(size_t index)
{
    return _playerList[index];
}

std::shared_ptr<Ego::Player> GameModule::tryGetPlayer(size_t index)
{
    return index < _playerList.size() ? _playerList[index] : nullptr;
}

std::shared_ptr<const Ego::Player> GameModule::tryGetPlayer(size_t index) const
{
    return index < _playerList.size() ? _playerList[index] : nullptr;
}

const std::vector<std::shared_ptr<Ego::Player>>& GameModule::getPlayerList() const
{
    return _playerList;
}
