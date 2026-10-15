#pragma once
#include "basedefs_de.h"
#include "server_de.h"
// Values retained from the original serverobj_de.h.
inline constexpr float PRECREATE_NORMAL = 0, PRECREATE_WORLDFILE = 1, PRECREATE_STRINGPROP = 2, PRECREATE_SAVEGAME = 3;
inline constexpr float INITIALUPDATE_NORMAL = 0, INITIALUPDATE_WORLDFILE = 1, INITIALUPDATE_STRINGPROP = 2,
                       INITIALUPDATE_SAVEGAME = 3;
inline constexpr DDWORD MID_PRECREATE = 0, MID_INITIALUPDATE = 1, MID_UPDATE = 2, MID_TOUCHNOTIFY = 3,
                        MID_LINKBROKEN = 4, MID_MODELSTRINGKEY = 5, MID_CRUSH = 6, MID_LOADOBJECT = 7,
                        MID_SAVEOBJECT = 8;
using AggregateEngineMessageFn = DDWORD (*)(LPBASECLASS, LPAGGREGATE, DDWORD, void*, float);
using AggregateObjectMessageFn = DDWORD (*)(LPBASECLASS, LPAGGREGATE, HOBJECT, DDWORD, HMESSAGEREAD);
