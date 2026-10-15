#pragma once
#include "Key.h"
#include <string>
// Original KeyData::Copy fields, with value-owned strings instead of HSTRINGs.
class KeyData
{
  public:
    bool Copy(const Key&);
    DRotation rotation;
    DVector position;
    float timestamp = 0, time = 0, sound_radius = 0;
    bool loop_sound = false;
    std::string name, sound, message_target, message, print;
};
