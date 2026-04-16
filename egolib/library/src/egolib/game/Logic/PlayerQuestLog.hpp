#pragma once

#include <string>

namespace Ego
{

class QuestLog;

// Load quest state from a player profile directory while preserving the
// current silent missing-file behavior of QuestLog::loadFromFile().
bool loadPlayerQuestLog(QuestLog& questLog, const std::string& profilePath);

} // namespace Ego
