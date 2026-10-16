#pragma once
#include "cpp_engineobjects_de.h"
#include "server_de.h"
// Original built-in Sound, AppHeaders/engineobjects.c:305-431, adapted from C callbacks.
class Sound : public BaseClass
{
  public:
    OpenBlood2::SoundRequest request;
    bool file_stream = false;
    OpenBlood2::SoundId sound = 0;
    std::string error;
    Sound();
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
};
