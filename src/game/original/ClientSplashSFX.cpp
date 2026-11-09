// Original ObjectDLL/ClientSplashSFX.cpp, using a typed detached client effect.
#include "ClientSplashSFX.h"

std::uint64_t CClientSplashSFX::Setup(const OpenBlood2::SplashSettings& settings, std::string& error)
{
    auto* server = GetServerDE();
    return server ? server->PublishSplash(m_hObject, settings, error) : 0;
}

DDWORD CClientSplashSFX::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    if (auto* server = GetServerDE())
    {
        if (message == MID_INITIALUPDATE)
            server->SetNextUpdate(m_hObject, .0001f);
        else if (message == MID_UPDATE)
            server->RemoveObject(m_hObject);
    }
    return CClientSFX::EngineMessageFn(message, data, reason);
}
