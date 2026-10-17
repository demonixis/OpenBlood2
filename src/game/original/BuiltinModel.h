#pragma once
#include "cpp_engineobjects_de.h"

namespace OpenBlood2
{
    // AppHeaders/engineobjects.c Model / mdl_MessageFn. The C++ name avoids
    // colliding with the decoded ABC asset; the registered class stays "Model".
    class BuiltinModel : public BaseClass
    {
      protected:
        DDWORD EngineMessageFn(DDWORD message, void* data, float reason) override;
    };
} // namespace OpenBlood2
