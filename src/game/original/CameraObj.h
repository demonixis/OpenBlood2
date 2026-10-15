#pragma once
#include "B2BaseClass.h"
#include "dlink.h"
#include <string>
class CameraObj : public B2BaseClass
{
  public:
    CameraObj();
    ~CameraObj() override;
    bool Setup(DBYTE);
    void SetActive(DBOOL);
    bool SetLinkObject(HOBJECT);
    HOBJECT GetLinkObject() const
    {
        return m_hLinkObject;
    }
    bool IsListener() const
    {
        return m_bIsListener;
    }
    bool IsActive() const
    {
        return m_bActive;
    }
    bool AllowPlayerMovement() const
    {
        return m_bPlayerMovement;
    }
    static DLink m_CameraHead;
    static DDWORD m_dwNumCameras;
    DLink m_Link{};
    bool m_bActive = false, m_bStartActive = false, m_bIsListener = true;
    float m_fActiveTime = -1, m_fDeactivateTime = 0;
    bool m_bPlayerMovement = false, m_bHidePlayer = false;
    DBYTE m_nType = 0;
    HOBJECT m_hLinkObject = nullptr;
    OpenBlood2::CameraSettings published;
    std::string last_command, tracking_error;
    std::uint64_t tracking_updates = 0;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;

  private:
    bool ReadProp();
    void InitialUpdate();
    void Update();
};
extern HOBJECT g_hActiveCamera;
