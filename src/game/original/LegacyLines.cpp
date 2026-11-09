// Selected ObjectDLL and ClientShellDLL cast-line and debug-line emitters.
#include "LegacyLines.h"
#include "engine/collision.h"
#include <cmath>
using namespace OpenBlood2;

bool CClientCastLineSFX::Setup(DVector start_color, DVector, float start_alpha, float end_alpha, HOBJECT target)
{
    auto* server = GetServerDE();
    DVector origin;
    DRotation rotation;
    if (configured_ || server->GetObjectPos(m_hObject, &origin) != DE_OK ||
        server->GetObjectRotation(m_hObject, &rotation) != DE_OK)
        return false;
    float length = 10000;
    if (target)
    {
        DVector end;
        if (!server->IsObjectAlive(target) || server->GetObjectPos(target, &end) != DE_OK)
            return false;
        const auto offset = vector_subtract(end, origin);
        length = std::sqrt(vector_dot(offset, offset));
    }
    else
    {
        SegmentHit hit;
        const auto result = server->CastRay(origin, rotate_vector(rotation, {0, 0, 1}), hit, m_hObject);
        if (result != SegmentResult::Hit && result != SegmentResult::Miss)
            return false;
        if (result == SegmentResult::Hit)
            length = std::sqrt(vector_dot(hit.point, hit.point)); // Original no-target absolute-point magnitude.
    }
    const auto end = vector_add(origin, rotate_vector(rotation, {0, 0, length}));
    // CCastLineFX copies its start colour into both vertices.
    if (!valid_line_effect({origin, end, start_color, start_color, start_alpha, end_alpha}) ||
        (target && server->CreateInterObjectLink(m_hObject, target) != DE_OK))
        return false;
    target_ = target;
    length_ = length;
    color_ = start_color;
    start_alpha_ = start_alpha;
    end_alpha_ = end_alpha;
    configured_ = true;
    return true;
}

bool CClientCastLineSFX::Geometry(DVector origin, const DRotation& rotation, LineEffectGeometry& line) const
{
    if (!configured_)
        return false;
    line = {origin, vector_add(origin, rotate_vector(rotation, {0, 0, length_})), color_, color_, start_alpha_,
            end_alpha_};
    return true;
}

DDWORD CClientCastLineSFX::EngineMessageFn(DDWORD msg, void* data, DFLOAT reason)
{
    if (msg == MID_LINKBROKEN && target_ && static_cast<HOBJECT>(data) == target_)
    {
        target_ = nullptr;
        GetServerDE()->RemoveObject(m_hObject);
    }
    return CClientSFX::EngineMessageFn(msg, data, reason);
}

bool CClientDebugLine::Setup(DVector from, DVector to)
{
    auto* server = GetServerDE();
    DVector origin;
    if (server->GetObjectPos(m_hObject, &origin) != DE_OK)
        return false;
    // The original serializes origin minus each endpoint as client-local coordinates.
    return server->PublishLine(m_hObject,
                               {vector_add(origin, vector_subtract(origin, from)),
                                vector_add(origin, vector_subtract(origin, to)), {1, 0, 0}, {1, 0, 0}, 1, 1},
                               20);
}

DDWORD CClientDebugLine::EngineMessageFn(DDWORD msg, void* data, DFLOAT reason)
{
    if (msg == MID_INITIALUPDATE)
        GetServerDE()->SetNextUpdate(m_hObject, .0001f);
    else if (msg == MID_UPDATE)
        GetServerDE()->RemoveObject(m_hObject);
    return CClientSFX::EngineMessageFn(msg, data, reason);
}
