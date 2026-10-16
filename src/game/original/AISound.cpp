// Original ObjectUtilities.cpp:284-358. Checked snapshots replace mutable list traversal.
#include "AISound.h"
#include "AI_Mgr.h"
#include "engine/collision.h"
#include <charconv>
#include <cmath>

bool SendSoundTrigger(HOBJECT sender, int kind, DVector position, float radius, OpenBlood2::SoundAlertReport& report)
{
    report = {};
    auto* server = BaseClass::GetServerDE();
    auto fail = [&](const char* why)
    {
        report.error = why;
        return false;
    };
    if (!server || !server->IsObjectAlive(sender) || kind < 0 || kind > 2 || !OpenBlood2::physics_vector(position) ||
        !std::isfinite(radius) || radius < 0)
        return fail("Invalid sound alert source, kind, position or radius");
    std::vector<HOBJECT> candidates;
    // Cabal first, then monsters. Preserve duplicates (players belong to both lists).
    for (const auto* head : {&AI_Mgr::m_CabalHead, &AI_Mgr::m_MonsterHead})
    {
        if (!head->m_pNext)
            continue;
        unsigned count = 0;
        for (auto* link = head->m_pNext; link != head; link = link->m_pNext)
        {
            if (!link || ++count > 65536)
                return fail("Invalid or over-limit AI sound list");
            auto h = server->ObjectToHandle(static_cast<BaseClass*>(link->m_pData));
            if (!h)
                return fail("AI sound list contains an unresolved object");
            candidates.push_back(h);
        }
    }
    std::string text = "SOUND_TRIGGER " + std::to_string(kind);
    for (auto value : {position.x, position.y, position.z})
    {
        char buffer[64];
        auto [end, ec] = std::to_chars(buffer, buffer + sizeof buffer, value, std::chars_format::fixed, 1);
        if (ec != std::errc{})
            return fail("Sound position formatting failed");
        text += ' ';
        text.append(buffer, end);
    }
    text += ';';
    const double squared = double(radius) * radius;
    for (auto h : candidates)
    {
        ++report.visited;
        if (h == sender || !server->IsObjectAlive(h))
        {
            ++report.skipped;
            continue;
        }
        if (!server->IsObjectAlive(sender))
            return fail("Sound sender removed during delivery");
        DVector target;
        if (server->GetObjectPos(h, &target) != DE_OK)
            return fail("Sound recipient position unavailable");
        const double x = double(target.x) - position.x, y = double(target.y) - position.y,
                     z = double(target.z) - position.z;
        if (x * x + y * y + z * z > squared)
            continue;
        ++report.eligible;
        auto string = server->CreateString(text.c_str());
        auto message = server->StartMessageToObject(server->HandleToObject(sender), h, 1000 /* MID_TRIGGER */);
        if (!string || !message || server->WriteToMessageHString(message, string) != DE_OK)
        {
            if (string)
                server->FreeString(string);
            if (message)
                server->EndHMessageWrite(message);
            return fail("Sound alert message allocation failed");
        }
        server->FreeString(string);
        if (server->EndMessage(message) != DE_OK)
            return fail("Sound alert delivery failed");
        ++report.delivered;
    }
    report.complete = true;
    return true;
}
