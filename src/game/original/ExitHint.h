#pragma once
// Selected original ExitHint.h, Monolith 1997; initialized links prevent rollback hazards.
#include "B2BaseClass.h"
#include "dlink.h"
class ExitHint : public B2BaseClass
{
  public:
    ExitHint()
    {
        if (!m_dwNumExits)
        {
            dl_TieOff(&m_ExitHead);
            m_ExitHead.m_pData = nullptr;
        }
    }
    ~ExitHint() override
    {
        if (m_Link.m_pData && m_dwNumExits)
        {
            dl_Remove(&m_Link);
            --m_dwNumExits;
        }
    }
    static DLink m_ExitHead;
    static DDWORD m_dwNumExits;
    DLink m_Link{};

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
};
