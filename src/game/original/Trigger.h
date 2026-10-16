#pragma once
#include "B2BaseClass.h"
#include <array>
#include <string>

// Selective original Trigger/BigTrigger/ToggleTrigger port. See TRIGGER_CONTRACTS.md.
class Trigger : public B2BaseClass
{
  public:
    struct Slot
    {
        std::string target, message, off_message;
        float delay = 0;
        bool pending = false;
        std::uint64_t sent = 0;
    };
    std::array<Slot, 10> slots;
    DVector dims{};
    float reset_time = 0, sound_radius = 200, max_delay = 0, delay_start = 0;
    bool active = true, touch_activate = true, player_activate = true, ai_activate = true;
    bool object_activate = false, relay_activate = true, named_activate = false, locked = false;
    std::string activation_name, activation_sound, locked_sound, unlocked_sound, key_name = "Key";
    std::string locked_message, unlocked_message, last_sound, last_command;
    std::uint32_t activation_count = 1, current_activation = 0;
    std::uint64_t activations = 0, rejected = 0, key_queries = 0, sound_requests = 0;
    HOBJECT last_sender = nullptr, owner_door = nullptr;
    void SetActive(bool value)
    {
        active = value;
    }

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;
    virtual void SendMessages();
    virtual void SendMessage(unsigned, const std::string&);

  private:
    bool ReadProp();
    bool ValidateSender(HOBJECT);
    void Activate(HOBJECT, bool touch);
    void Update();
    void Sound(const std::string&);
    bool activating_ = false;
    HOBJECT querying_ = nullptr;
};
class BigTrigger : public Trigger
{
};
class ToggleTrigger : public Trigger
{
  public:
    bool on = false;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    void SendMessages() override;
};
