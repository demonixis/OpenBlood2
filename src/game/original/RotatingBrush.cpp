#include "RotatingBrush.h"
RotatingBrush::RotatingBrush()
{
    AddAggregate(&rotating);
    allow_marks = false;
}
DDWORD RotatingBrush::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    if (message == MID_INITIALUPDATE)
        rotating.Init(m_hObject);
    const auto accepted = CDestructableBrush::EngineMessageFn(message, data, reason);
    if (message == MID_PRECREATE)
        return accepted && rotating.valid && data && !(static_cast<ObjectCreateStruct*>(data)->m_Flags & FLAG_GRAVITY);
    return accepted;
}
