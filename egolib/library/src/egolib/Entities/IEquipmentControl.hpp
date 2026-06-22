#pragma once

class IEquipmentControl
{
public:
    virtual ~IEquipmentControl() = default;

    virtual void setEquipped(bool equipped) = 0;
};
