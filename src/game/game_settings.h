#pragma once
// Values from original Shared/SharedDefs.h and Shared/NetDefs.h.
enum GameType
{
    GAMETYPE_SINGLE = 0,
    GAMETYPE_BLOODBATH,
    GAMETYPE_CTF,
    GAMETYPE_COOP,
    GAMETYPE_TEAMPLAY,
    GAMETYPE_SOCCER,
    GAMETYPE_TAG,
    GAMETYPE_LEVELRACE,
    GAMETYPE_ACTION,
    GAMETYPE_CUSTOM
};
enum SaveType
{
    SAVETYPE_AUTO = 0,
    SAVETYPE_CURRENT
};
enum Difficulty
{
    DIFFICULTY_EASY = 0,
    DIFFICULTY_MEDIUM,
    DIFFICULTY_HARD,
    DIFFICULTY_INSANE
};
