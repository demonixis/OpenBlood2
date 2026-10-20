#include "RotatingModel.h"

RotatingModel::RotatingModel()
{
    rotating.model_rotation = true;
    AddAggregate(&rotating);
}

DDWORD RotatingModel::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    if (message == MID_INITIALUPDATE)
        rotating.Init(m_hObject);
    const auto accepted = CDestructableModel::EngineMessageFn(message, data, reason);
    return message == MID_PRECREATE ? accepted && rotating.valid : accepted;
}
