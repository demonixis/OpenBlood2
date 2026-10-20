// Synthetic audit probe: compiles the unchanged OpenLithTech core.
// Frame time and the unused global server pointer are the only link shims.
#include "engine.h"
#include "class_manager.h"
#include <lithtech/baseclass.h>
#include <cstdio>
#include <cstring>

ServerDE* g_pServerDE = nullptr;
extern "C" void LT_SetFrameTime(DFLOAT) {}

static bool removeDuringUpdate = false;
static float initialReason = -1;
static unsigned updates = 0;

static DDWORD Callback(void* instance, DDWORD message, void* data, DFLOAT reason)
{
    auto& engine = LithTech::Engine::Instance();
    auto* object = static_cast<BaseClass*>(instance);
    if (message == MID_PRECREATE) {
        static_cast<ObjectCreateStruct*>(data)->m_Pos.x = 42.0f;
    }
    if (message == MID_INITIALUPDATE) initialReason = reason;
    if (message == MID_UPDATE) {
        ++updates;
        if (removeDuringUpdate) {
            engine.RemoveObject(object->m_hObject);
        } else {
            // Same field assignment as GameInterface::SetNextUpdate.
            engine.GetObject(object->m_hObject)->m_fNextUpdate = engine.GetTime() + 0.01f;
        }
    }
    return 1;
}

int main(int argc, char** argv)
{
    removeDuringUpdate = argc > 1 && std::strcmp(argv[1], "remove") == 0;
    auto& engine = LithTech::Engine::Instance();
    auto cls = LithTech::ClassManager::Instance().RegisterClass(
        "AuditSynthetic", [] { return new BaseClass(); }, nullptr, nullptr, Callback);
    ObjectCreateStruct create{};
    INIT_OBJECTCREATESTRUCT(create);
    engine.SetPendingProperties({{"AuditSyntheticProperty", "true"}});
    auto handle = engine.CreateObject(cls, &create);
    engine.ClearPendingProperties();
    std::printf("precreate_struct_x=%.2f stored_object_x=%.2f initial_reason=%.0f\n",
        create.m_Pos.x, engine.GetObject(handle)->m_vPos.x, initialReason);
    std::fflush(stdout);
    engine.Update(0.02f);
    if (!removeDuringUpdate) {
        std::printf("callback_requested_delay=0.01 stored_delay=%.2f\n",
            engine.GetObject(handle)->m_fNextUpdate - engine.GetTime());
        engine.Update(0.02f);
        std::printf("updates_after_two_20ms_frames=%u\n", updates);
        engine.RemoveObject(handle);
    }
}
