#pragma once

#include "egolib/Graphics/AnimatedModel.hpp"

#include <memory>
#include <string>

class MD2Model
{
public:
    MD2Model() = delete;

    static std::shared_ptr<Ego::Graphics::AnimatedModel> loadFromFile(const std::string& fileName);
};
