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
    return message == MID_PRECREATE ? accepted && rotating.valid : accepted;
}
