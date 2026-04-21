#pragma once

#include "egolib/game/egoboo.h"

#include <memory>
#include <vector>

class Camera;
class ego_mesh_t;

class ICameraSystem
{
public:
    virtual ~ICameraSystem() = default;

    virtual void updateAll(const ego_mesh_t* mesh) = 0;
    virtual void setNumberOfCameras(size_t numberOfCameras) = 0;
    virtual const std::vector<std::shared_ptr<Camera>>& getCameraList() const = 0;
};
