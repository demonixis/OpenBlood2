#pragma once
#include "cpp_aggregate_de.h"
#include <array>
#include <string>
class CDebris : public Aggregate
{
  public:
    bool Init(HOBJECT);
    void Create(DVector direction, float damage);
    unsigned GetType() const
    {
        return surface;
    }
    HOBJECT object = nullptr;
    // Source emission order also defines the first selected surface.
    static constexpr std::array<const char*, 9> names{"Stone", "Metal", "Wood",   "Terrain", "Plastic",
                                                      "Glass", "Flesh", "Liquid", "Energy"};
    static constexpr std::array<unsigned, 9> surfaces{10, 20, 30, 70, 90, 50, 100, 200, 40};
    std::array<bool, 9> materials{};
    bool exploding = false, custom = false, valid = true;
    DBYTE amount = 10;
    float scale = 1;
    // Original ReadProp never reads the corresponding authored properties.
    static constexpr float explosion_damage = 100, explosion_radius = 200;
    unsigned surface = 0;
    std::string model, texture, sound;

  protected:
    DDWORD EngineMessageFn(LPBASECLASS, DDWORD, void*, DFLOAT) override;
};
