#include "egolib/game/Module/Fog.hpp"
#include "egolib/game/Core/EngineContext.hpp"

void fog_instance_t::upload(const wawalite_fog_t& source)
{
    _on = source.found && EngineContext::get().config().graphic_fog_enable.getValue();
    _top = source.top;
    _bottom = source.bottom;

    _red = source.red;
    _grn = source.grn;
    _blu = source.blu;

    _distance = (source.top - source.bottom);

    _on = (_distance < 1.0f) && _on;
}
