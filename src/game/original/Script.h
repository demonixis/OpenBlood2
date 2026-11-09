#pragma once
#include "B2BaseClass.h"
#include <array>
#include <vector>

class Script final : public B2BaseClass
{
  public:
    std::uint64_t executed = 0, failures = 0;
    std::string error;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;

  private:
    enum class Op
    {
        Start, End, Wait, Assign, Create, CreateNear, CreateModel, CreateModelNear, CreateFire, ExplosionAt,
        ExplosionNear, Move, MoveTo, Teleport, Flags, Visible, Solid, Gravity, Dims, Scale, Destroy, Gib, Animation,
        SoundAt, SoundOn, KillSound, Trigger, TriggerNamed
    };
    struct Command
    {
        Op op;
        unsigned line;
        std::array<std::string, 8> text;
        std::array<float, 8> number{};
        unsigned Slot(unsigned arg = 0) const { return static_cast<unsigned>(number[arg]); }
        DVector Vector(unsigned arg) const { return {number[arg], number[arg + 1], number[arg + 2]}; }
    };
    struct Motion
    {
        HOBJECT object = nullptr;
        unsigned slot = 0;
        unsigned line = 0;
        DVector from, to;
        DRotation from_rotation, to_rotation;
        float start = 0, duration = 0;
    };
    bool Prepare();
    bool SetSlot(unsigned, HOBJECT);
    HOBJECT Create(std::string_view class_name, ObjectCreateStruct&);
    bool NearPosition(HOBJECT, DVector&);
    bool Send(HOBJECT, const std::string&);
    bool PlaySound(const Command&);
    bool StartMotion(HOBJECT, const Command&);
    bool MoveToward(const Motion&, float time);
    void RecordFailure(unsigned line);
    bool Execute(const Command&);
    void Update();
    std::string filename_;
    std::vector<Command> commands_;
    std::array<HOBJECT, 30> objects_{};
    std::array<OpenBlood2::SoundId, 30> sounds_{};
    std::array<Motion, 10> motions_{};
    std::size_t next_ = 0;
    float delay_ = .001f, next_time_ = 0;
};
