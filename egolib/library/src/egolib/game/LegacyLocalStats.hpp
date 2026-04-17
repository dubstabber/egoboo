#pragma once

#include "egolib/IDSZ.hpp"
#include "egolib/typedef.h"

// Legacy compatibility mirrors owned and synchronized by GameSessionContext.
// Keep direct includes of this header narrow so new code prefers the session-owned APIs.
struct local_stats_t
{
    bool noplayers;          ///< Are there any local players?
    int player_count;

    float grog_level;
    float daze_level;
    float seeinvis_level;
    float seeinvis_mag;
    float seedark_level;
    float seedark_mag;
    float seekurse_level;

    bool allpladead;         ///< Have players died?
    int revivetimer;         ///< Legacy compatibility mirror for the session-owned respawn cooldown

    // ESP
    TEAM_REF sense_enemies_team;
    IDSZ2 sense_enemies_idsz;
};

extern local_stats_t local_stats;
