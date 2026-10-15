#pragma once
#include "B2BaseClass.h"
// Original CClientSFX callback. ClientLightFX explicitly calls B2BaseClass instead.
class CClientSFX : public B2BaseClass
{
  public:
    explicit CClientSFX(DBYTE type = OT_NORMAL) : B2BaseClass(type) {}

  protected:
    DDWORD EngineMessageFn(DDWORD message, void* data, DFLOAT reason) override
    {
        if (message == MID_PRECREATE && data)
            static_cast<ObjectCreateStruct*>(data)->m_Flags |= FLAG_FORCECLIENTUPDATE;
        return B2BaseClass::EngineMessageFn(message, data, reason);
    }
};
