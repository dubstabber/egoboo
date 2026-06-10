#include "egolib/game/Module/Fog.hpp"
#include "egolib/egoboo_setup.h"

void fog_instance_t::upload(const wawalite_fog_t& source)
{
    _on = source.found && Ego::activeConfig().graphic_fog_enable.getValue();
    _top = source.top;
    _bottom = source.bottom;

    _red = source.red;
    _grn = source.grn;
    _blu = source.blu;

    _distance = (source.top - source.bottom);

    _on = (_distance < 1.0f) && _on;
}
