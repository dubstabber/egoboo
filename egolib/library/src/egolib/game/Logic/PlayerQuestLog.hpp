#pragma once

#include <string>

namespace Ego
{

class QuestLog;

// Load quest state from a player profile directory. QuestLog::loadFromFile() returns
// false with the quest log cleared for both a missing quest.txt (silently, as before)
// and a present-but-malformed quest.txt (additionally reported through the active log
// target when one is available); neither case throws (see QuestLog.hpp's loadFromFile()
// contract).
bool loadPlayerQuestLog(QuestLog& questLog, const std::string& profilePath);

} // namespace Ego
