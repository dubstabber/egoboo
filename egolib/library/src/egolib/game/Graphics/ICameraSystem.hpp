#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "egolib/typedef.h"  // ObjectRef

class Camera;
class ego_mesh_t;
struct CameraOptions;

namespace Ego {
namespace Graphics {
struct TileList;
struct EntityList;
} // namespace Graphics
} // namespace Ego

class ICameraSystem
{
public:
    virtual ~ICameraSystem() = default;

    virtual void updateAll(const ego_mesh_t* mesh) = 0;
    virtual void setNumberOfCameras(size_t numberOfCameras) = 0;
    virtual const std::vector<std::shared_ptr<Camera>>& getCameraList() const = 0;

    /// @brief Get the main camera (the camera at index 0).
    virtual std::shared_ptr<Camera> getMainCamera() const = 0;

    /// @brief Get the first camera tracking @a targetRef, or the main camera if none tracks it.
    virtual std::shared_ptr<Camera> getCamera(ObjectRef targetRef) const = 0;

    /// @brief Write access to the global camera options.
    virtual CameraOptions& getCameraOptions() = 0;

    /// @brief Render all cameras using @a renderFunction.
    virtual void renderAll(std::function<void(std::shared_ptr<Camera>, std::shared_ptr<Ego::Graphics::TileList>, std::shared_ptr<Ego::Graphics::EntityList>)> renderFunction) = 0;
};
