#pragma once
#include "engine/arm_ik.h"
#include <array>
#include <string_view>
#include <vector>
namespace OpenBlood2
{
    // Arm chains of an original first-person weapon model (`*_pv.abc`): an upper arm `lu_arm*`,
    // its lower arm `ll_arm*`, the wrist null `d_null*` under that and the hand mesh below.
    struct ViewArmChain
    {
        std::uint16_t upper = 0, lower = 0, wrist = 0;
        bool right = false; // Body side, from the upper arm's rest x.
    };
    // What the controller holds: the hand of the chain on the model's own side, the audited grip
    // of a two-handed model (its trigger hand is not modelled), else the model's bounds centre.
    enum class ViewGrip
    {
        Hand,
        Audited,
        Bounds
    };
    struct ViewArmRig
    {
        std::vector<ViewArmChain> chains;
        int held = -1; // The chain whose hand holds the controller.
        ViewGrip grip = ViewGrip::Bounds;
        DVector anchor{}; // Rest model-space point held at the controller grip.
    };
    // One view-model unit in metres: the PV models are about 1/8 scale (hand ~15 cm, Beretta
    // ~22 cm, forearm ~29 cm at this value).
    inline constexpr float view_model_meters = .17f;
    // Rest pose: static_model, else idle, fire, then the first clip, at time zero.
    bool rig_view_arms(const Model&, std::string_view model_path, bool left, ViewArmRig&);
    // A view model held by a tracked controller, in world space.
    struct ViewArmPlacement
    {
        NodePose grip;                      // Grip position, aim orientation.
        float scale = 1;                    // World units per model unit.
        std::array<DVector, 2> shoulders{}; // Virtual shoulders {left, right}.
        std::array<DVector, 2> poles{};     // Elbow directions {left, right}.
    };
    struct ViewArmPose
    {
        NodePose model;                   // World pose of the model (drawn at `scale`).
        std::vector<ArmTarget> targets;   // Model space, one per visible chain.
        std::vector<std::uint8_t> hidden; // Model order.
    };
    // The model is turned to the aim and moved so its anchor sits on the grip. What the original
    // camera framed out (offset: the original view-model offset, mirrored for the left hand) stays
    // hidden: other chains whose hand lies wholly outside the widened view, and any node outside
    // the chains whose vertices all do. The held chain is always drawn.
    bool pose_view_arms(const Model&, const AnimationTrack&, const ViewArmRig&, const ViewArmPlacement&, DVector offset,
                        ViewArmPose&);
} // namespace OpenBlood2
