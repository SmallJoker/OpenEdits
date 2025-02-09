#pragma once

#include <cstdint> // uint16_t
#include <map>

typedef uint16_t event_id_t;

struct ScriptEventData;

using ScriptEvent    = std::pair<event_id_t, ScriptEventData>;
using ScriptEventMap = std::map <event_id_t, ScriptEventData>;
