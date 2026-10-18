#pragma once
#include "cpp_engineobjects_de.h"
#include <string>

namespace OpenBlood2
{
    // AppHeaders/engineobjects.c Sprite / spr_MessageFn. The C++ name avoids
    // colliding with the decoded SPR manifest; the registered class stays "Sprite".
    class BuiltinSprite : public BaseClass
    {
      public:
        DBYTE m_Color[4]{}; // Original RGBA bytes.
        bool color_applied = false;
        std::string error;

      protected:
        DDWORD EngineMessageFn(DDWORD message, void* data, float reason) override;
    };
} // namespace OpenBlood2
