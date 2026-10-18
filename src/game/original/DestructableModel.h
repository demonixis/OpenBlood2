#pragma once
#include "B2BaseClass.h"
#include "Debris.h"
#include "Destructable.h"

// Original ObjectDLL/DestructableModel.cpp, typed direct-integration adaptation.
class CDestructableModel : public B2BaseClass
{
  public:
    CDestructableModel();
    ~CDestructableModel() override;
    CDestructable m_damage;
    CDebris debris;
    bool destructable = true, pushable = false, dead_state = false, initialized = false;
    float hit_points = 100, damage_hit_points = 50, mass = 100, scale = 1;
    float alpha = 1, target_alpha = 1, fade_rate = 0, last_time = 0;
    DVector init_dims{}, damage_dims{}, destroy_dims{}, tint{}, last_position{};
    DDWORD destroy_flags = FLAG_VISIBLE | FLAG_SOLID | FLAG_GRAVITY;
    unsigned surface = 0, dimensions_publications = 0, destructions = 0;
    std::string damage_model, damage_skin, destroy_model, destroy_skin, sliding_path;
    std::string error, trigger_error, sound_error;
    bool sliding = false, standing = false;
    unsigned sliding_frames = 0, standing_frames = 0;
    OpenBlood2::SoundId sliding_sound = 0;
    DRESULT ground_result = DE_NOTFOUND;
    bool HandleTriggerMessage(std::string_view);

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;

  private:
    bool destroying_ = false;
    bool ReadProp(ObjectCreateStruct&);
    bool Reject(const std::string& reason); // Explained precreate refusal.
    bool InitialUpdate(float);
    bool SetDimensions(DVector&, bool rotate, float shadow_limit);
    bool ChangeModel(const std::string&, const std::string&, DVector&, bool destroyed);
    void HandleDamage();
    void HandleDestruction();
    void Update();
    void TouchNotify(HOBJECT, float);
    bool Fail(const char*);
};
