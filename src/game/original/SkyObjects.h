#pragma once
#include "cpp_engineobjects_de.h"
#include "engine/sky.h"
#include "server_de.h"
// Original AppHeaders/engineobjects.c sky callbacks, adapted to typed C++ objects.
class SkyObject : public BaseClass
{
  public:
    DVector dimensions{}, inner{.1f, .1f, .1f};
    std::int32_t index = 0;
    std::string target, error;
    bool pointer = false;
    explicit SkyObject(bool is_pointer) : pointer(is_pointer) {}
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
};
class DemoSkyWorldModel : public SkyObject
{
  public:
    DemoSkyWorldModel() : SkyObject(false) {}
};
class SkyPointer : public SkyObject
{
  public:
    SkyPointer() : SkyObject(true) {}
};
