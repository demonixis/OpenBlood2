// Selected original ObjectDLL/DetailSprite.cpp. Value-owned filename; bounded repeat work.
#include "DetailSprite.h"
#include "SaveFields.h"
#include "engine/collision.h"
#include <algorithm>
using namespace OpenBlood2;
bool DetailSprite::ReadProp()
{
    GenericProp p;
    if (g_pServerDE->GetPropGeneric("Filename", &p) == DE_OK)
        filename = p.m_String;
    g_pServerDE->GetPropReal("ScaleX", &scale.x);
    g_pServerDE->GetPropReal("ScaleY", &scale.y);
    g_pServerDE->GetPropReal("SpacingX", &spacing_x);
    g_pServerDE->GetPropReal("SpacingY", &spacing_y);
    auto boolean = [&](const char* name, bool& out)
    {
        if (g_pServerDE->GetPropGeneric(name, &p) == DE_OK)
            out = p.m_Bool != 0;
    };
    boolean("Rotatable", rotatable);
    boolean("FlushWithWorld", flush);
    boolean("Chromakey", chromakey);
    boolean("Repeat", repeat);
    if (g_pServerDE->GetPropGeneric("RepeatX", &p) == DE_OK)
        repeat_x = static_cast<DDWORD>(p.m_Long);
    if (g_pServerDE->GetPropGeneric("RepeatY", &p) == DE_OK)
        repeat_y = static_cast<DDWORD>(p.m_Long);
    // Source signed-to-unsigned counts can otherwise produce unbounded creation.
    return repeat_x <= 1024 && repeat_y <= 1024 && std::uint64_t(std::max(1u, repeat_x)) * repeat_y <= 1024 &&
           physics_vector(scale) && physics_vector({spacing_x, spacing_y, 0});
}
DDWORD DetailSprite::EngineMessageFn(DDWORD message, void* data, DFLOAT reason)
{
    if (message == MID_PRECREATE)
    {
        if (!data)
            return 0;
        if (reason == PRECREATE_WORLDFILE || reason == PRECREATE_STRINGPROP)
        {
            if (!ReadProp())
                return 0;
            auto& state = *static_cast<ObjectCreateStruct*>(data);
            state.m_SkinName[0] = 0;
            state.m_NextUpdate = .1f;
            state.m_Scale = scale;
            state.m_Flags |= FLAG_VISIBLE;
            if (rotatable || flush)
                state.m_Flags |= FLAG_ROTATEABLESPRITE;
            if (chromakey)
                state.m_Flags |= FLAG_SPRITECHROMAKEY;
            if (!filename.empty())
                legacy_copy(state.m_Filename, filename.c_str());
        }
    }
    else if (message == MID_INITIALUPDATE && reason != INITIALUPDATE_SAVEGAME && !InitialUpdate())
        g_pServerDE->RemoveObject(m_hObject);
    else if (message == MID_SAVEOBJECT && data)
        Save(*static_cast<HMESSAGEWRITE*>(data));
    else if (message == MID_LOADOBJECT && data)
        Load(*static_cast<HMESSAGEREAD*>(data));
    return B2BaseClass::EngineMessageFn(message, data, reason);
}
bool DetailSprite::InitialUpdate()
{
    g_pServerDE->SetNextUpdate(m_hObject, 0);
    g_pServerDE->SetObjectUserFlags(m_hObject, g_pServerDE->GetObjectUserFlags(m_hObject) | (1u << 3));
    if (!flush)
        return true;
    DVector pos;
    DRotation rotation;
    g_pServerDE->GetObjectPos(m_hObject, &pos);
    g_pServerDE->GetObjectRotation(m_hObject, &rotation);
    auto up = rotate_vector({0, 1, 0}, rotation), right = rotate_vector({1, 0, 0}, rotation);
    flush_result = g_pServerDE->CastWorldRay(pos, rotate_vector({0, 0, 1}, rotation), flush_hit);
    if (flush_result == SegmentResult::Unavailable || flush_result == SegmentResult::Invalid)
    {
        error = "DetailSprite flush ray unavailable/invalid";
        return false;
    }
    if (flush_result == SegmentResult::Hit)
    {
        pos = vector_add(flush_hit.point, vector_scale(flush_hit.normal, .25f));
        if (g_pServerDE->TeleportObject(m_hObject, &pos) != DE_OK)
        {
            error = "DetailSprite flush placement rejected";
            return false;
        }
        // Original assigns a local forward vector, but never changes object rotation.
    }
    if (!repeat)
        return true;
    up = vector_scale(up, spacing_y);
    right = vector_scale(right, spacing_x);
    auto column = pos;
    for (unsigned y = 1; y < repeat_y; ++y)
    {
        column = vector_add(column, up);
        if (!CreateAnotherSprite(column, rotation))
            return false;
    }
    column = pos;
    for (unsigned x = 1; x < repeat_x; ++x)
    {
        column = vector_add(column, right);
        auto row = column;
        for (unsigned y = 0; y < repeat_y; ++y)
        {
            if (!CreateAnotherSprite(row, rotation))
                return false;
            row = vector_add(row, up);
        }
    }
    return true;
}
bool DetailSprite::CreateAnotherSprite(DVector position, DRotation rotation)
{
    if (filename.empty())
        return true;
    if (!physics_vector(position))
    {
        error = "DetailSprite repeat position outside physics bounds";
        return false;
    }
    ObjectCreateStruct state;
    legacy_copy(state.m_Filename, filename.c_str());
    state.m_Pos = position;
    state.m_Rotation = rotation;
    state.m_Scale = scale;
    state.m_Flags = g_pServerDE->GetObjectFlags(m_hObject);
    auto* child = g_pServerDE->CreateObject(g_pServerDE->GetClass("DetailSprite"), &state);
    if (!child)
    {
        error = "DetailSprite repeat creation failed";
        return false;
    }
    children.push_back(child->m_hObject);
    return true;
}
void DetailSprite::Save(HMESSAGEWRITE h)
{
    // Original DetailSprite.cpp:322-334. The repeat settings are init-only: repeats are
    // independent DetailSprites (with no filename member) that save themselves. The engine
    // record restores the sprite, its frame clock, scale, flags and placement.
    auto* s = g_pServerDE;
    s->WriteToMessageVector(h, &scale);
    save_fields::write_string(h, filename);
    s->WriteToMessageByte(h, rotatable);
    s->WriteToMessageByte(h, chromakey);
    s->WriteToMessageByte(h, aligned_to_world);
    s->WriteToMessageByte(h, flush);
}
void DetailSprite::Load(HMESSAGEREAD h)
{
    // Original DetailSprite.cpp:344-356, decoded and checked before any member changes.
    auto* s = g_pServerDE;
    namespace F = save_fields;
    DVector saved_scale{};
    s->ReadFromMessageVector(h, &saved_scale);
    std::string saved_filename;
    bool flags[4]{};
    bool ok = F::read_string(h, saved_filename);
    for (auto& flag : flags)
        ok = F::read_bool(h, flag) && ok;
    if (!ok)
        return;
    if (!physics_vector(saved_scale))
    {
        s->RejectPayload(h, "detail sprite scale is outside the physics bounds");
        return;
    }
    scale = saved_scale;
    filename = std::move(saved_filename);
    rotatable = flags[0];
    chromakey = flags[1];
    aligned_to_world = flags[2];
    flush = flags[3];
}
