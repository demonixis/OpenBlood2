// Original PickupObject.cpp:617-674 and ItemPickups.cpp:480-511 (KeyPickup) Save/Load bodies
// (save_load_contracts lot 9). Loads decode and check every field before a member changes.
#include "KeyPickup.h"
#include "PickupObject.h"
#include "SaveFields.h"
#include "engine/collision.h"
#include <optional>
using namespace OpenBlood2;
namespace F = OpenBlood2::save_fields;
namespace
{
    // PickupObject's block. m_bFirstUpdate, m_fRespawnTime (single player), m_bRotate, m_fYaw and
    // m_fLastTime never change in the original's single-player game.
    struct PickupBlock
    {
        bool first_update = true;
        DBYTE type = 0;
        float value = 0, respawn = 0;
        bool rotate = false;
        float yaw = 0, last_time = 0; // last_time: the absolute 0 written relative.
        DDWORD flags = 0;
        float bounce = 0;
        bool bouncing = false;
        DVector rest{};
        std::string target, message;
        std::optional<std::string> object_name, display_name;
    };
    void write_block(HMESSAGEWRITE h, const PickupBlock& b)
    {
        auto* s = g_pServerDE;
        s->WriteToMessageByte(h, b.first_update);
        s->WriteToMessageByte(h, b.type);
        s->WriteToMessageFloat(h, b.value);
        s->WriteToMessageFloat(h, b.respawn);
        s->WriteToMessageByte(h, b.rotate);
        s->WriteToMessageFloat(h, b.yaw);
        s->WriteToMessageFloat(h, b.last_time - s->GetTime());
        s->WriteToMessageDWord(h, b.flags);
        s->WriteToMessageFloat(h, b.bounce);
        s->WriteToMessageByte(h, b.bouncing);
        s->WriteToMessageVector(h, &b.rest);
        F::write_string(h, b.target);
        F::write_string(h, b.message);
        F::write_nullable(h, b.object_name ? b.object_name->c_str() : nullptr);
        F::write_nullable(h, b.display_name ? b.display_name->c_str() : nullptr);
    }
    bool read_block(HMESSAGEREAD h, PickupBlock& b)
    {
        auto* s = g_pServerDE;
        bool ok = F::read_bool(h, b.first_update);
        b.type = s->ReadFromMessageByte(h);
        b.value = s->ReadFromMessageFloat(h);
        b.respawn = s->ReadFromMessageFloat(h);
        ok = F::read_bool(h, b.rotate) && ok;
        b.yaw = s->ReadFromMessageFloat(h);
        b.last_time = s->ReadFromMessageFloat(h);
        b.flags = s->ReadFromMessageDWord(h);
        b.bounce = s->ReadFromMessageFloat(h);
        ok = F::read_bool(h, b.bouncing) && ok;
        s->ReadFromMessageVector(h, &b.rest);
        ok = F::read_string(h, b.target) && ok;
        ok = F::read_string(h, b.message) && ok;
        ok = F::read_nullable(h, b.object_name) && ok;
        ok = F::read_nullable(h, b.display_name) && ok;
        return ok;
    }
    // The fields every single-player pickup writes unchanged; the relative last time is the
    // original's absolute 0 against the restored clock.
    std::string check_fixed(const PickupBlock& b)
    {
        if (!b.first_update || b.respawn != 0 || b.rotate || b.yaw != 0 || b.last_time != 0.0f - g_pServerDE->GetTime())
            return "pickup block holds multiplayer or rotation state the port does not model";
        if (!F::finite(b.bounce) || !physics_vector(b.rest))
            return "pickup bounce state cannot be represented";
        return {};
    }
    std::optional<std::string> nullable(const std::string& text)
    {
        return text.empty() ? std::nullopt : std::optional<std::string>(text);
    }
} // namespace

void PickupObject::Save(HMESSAGEWRITE h)
{
    if (picked_up)
    {
        g_pServerDE->RejectPayload(h, "a collected pickup cannot be saved");
        return;
    }
    PickupBlock b;
    b.type = value.type;
    b.value = value.amount;
    b.flags = m_dwFlags;
    b.bounce = bounce_acceleration;
    b.bouncing = bouncing;
    b.rest = rest_position;
    b.target = pickup_target;
    b.message = pickup_command;
    b.object_name = display_name; // Original m_szObjectName; the port keeps no resource display name.
    write_block(h, b);
}
void PickupObject::Load(HMESSAGEREAD h)
{
    auto* s = g_pServerDE;
    PickupBlock b;
    if (!read_block(h, b))
        return;
    auto reason = check_fixed(b);
    if (reason.empty() && b.type != value.type)
        reason = "saved pickup type " + std::to_string(b.type) + " differs from the class's";
    if (reason.empty() && (!F::finite(b.value) || b.value < 0))
        reason = "saved pickup value cannot be represented";
    if (reason.empty() && (b.object_name != display_name || b.display_name))
        reason = "saved pickup names differ from the class's (the port has no resource display name)";
    if (!reason.empty())
    {
        s->RejectPayload(h, reason);
        return;
    }
    value.amount = b.value;
    m_dwFlags = b.flags;
    bounce_acceleration = b.bounce;
    bouncing = b.bouncing;
    rest_position = b.rest;
    pickup_target = std::move(b.target);
    pickup_command = std::move(b.message);
}

void KeyPickup::Save(HMESSAGEWRITE h)
{
    auto* s = g_pServerDE;
    if (picked_up)
    {
        s->RejectPayload(h, "a collected key cannot be saved");
        return;
    }
    F::write_string(h, model_file);
    F::write_string(h, model_skin);
    F::write_string(h, item.icon);
    F::write_string(h, item.highlighted_icon);
    s->WriteToMessageByte(h, item.uses);
    PickupBlock b; // KeyPickup sets no type, value or bounce.
    b.flags = m_dwFlags;
    b.target = pickup_target;
    b.message = pickup_command;
    b.object_name = nullable(item.name);
    b.display_name = nullable(item.display_name); // A ResourceNum key shows its name in the port.
    write_block(h, b);
}
void KeyPickup::Load(HMESSAGEREAD h)
{
    auto* s = g_pServerDE;
    std::string model, skin, icon, icon_h;
    bool ok = F::read_string(h, model);
    ok = F::read_string(h, skin) && ok;
    ok = F::read_string(h, icon) && ok;
    ok = F::read_string(h, icon_h) && ok;
    const DBYTE uses = s->ReadFromMessageByte(h);
    PickupBlock b;
    if (!read_block(h, b) || !ok)
        return;
    auto reason = check_fixed(b);
    if (reason.empty() && (b.type || b.value != 0 || b.bounce != 0 || b.bouncing))
        reason = "saved key holds pickup state a key never sets";
    if (!reason.empty())
    {
        s->RejectPayload(h, reason);
        return;
    }
    model_file = std::move(model);
    model_skin = std::move(skin);
    item.icon = std::move(icon);
    item.highlighted_icon = std::move(icon_h);
    item.uses = uses;
    item.name = b.object_name.value_or("");
    item.display_name = b.display_name.value_or("");
    m_dwFlags = b.flags;
    pickup_target = std::move(b.target);
    pickup_command = std::move(b.message);
}
