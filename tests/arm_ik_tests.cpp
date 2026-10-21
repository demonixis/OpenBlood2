#include "engine/arm_ik.h"
#include "engine/config.h"
#include "engine/model_view.h"
#include "engine/objects.h"
#include "engine/resources.h"
#include "engine/view_arms.h"
#include "game/original/WeaponDefs.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <string_view>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __func__ << ':' << __LINE__ << ": " #x "\n";                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    DVector sub(DVector a, DVector b)
    {
        return {a.x - b.x, a.y - b.y, a.z - b.z};
    }
    DVector add(DVector a, DVector b)
    {
        return {a.x + b.x, a.y + b.y, a.z + b.z};
    }
    DVector scaled(DVector a, float s)
    {
        return {a.x * s, a.y * s, a.z * s};
    }
    float dot(DVector a, DVector b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    DVector cross(DVector a, DVector b)
    {
        return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
    }
    float length(DVector a)
    {
        return std::sqrt(dot(a, a));
    }
    bool near(DVector a, DVector b, float epsilon = 1e-4f)
    {
        return length(sub(a, b)) <= epsilon;
    }
    bool same(const NodePose& a, const NodePose& b)
    {
        return a.position.x == b.position.x && a.position.y == b.position.y && a.position.z == b.position.z &&
               a.rotation.m_Vec.x == b.rotation.m_Vec.x && a.rotation.m_Vec.y == b.rotation.m_Vec.y &&
               a.rotation.m_Vec.z == b.rotation.m_Vec.z && a.rotation.m_Spin == b.rotation.m_Spin;
    }
    DRotation conjugate(DRotation q)
    {
        return {{-q.m_Vec.x, -q.m_Vec.y, -q.m_Vec.z}, q.m_Spin};
    }
    // `child` in the frame of `parent`: unchanged when both move rigidly together.
    DVector local(const NodePose& parent, DVector point)
    {
        return rotate_vector(conjugate(parent.rotation), sub(point, parent.position));
    }
    DRotation turn(float pitch, float yaw, float roll)
    {
        return rotation_from_euler({pitch, yaw, roll});
    }

    // root > upper > {sleeve, lower > wrist > hand}: a bent arm along +Z, elbow below.
    struct Chain
    {
        Model model;
        std::vector<NodePose> pose;
    };
    Chain chain()
    {
        Chain c;
        for (int parent : {-1, 0, 1, 1, 3, 4})
        {
            c.model.nodes.push_back({});
            c.model.nodes.back().parent = parent;
            c.model.nodes.back().index = static_cast<std::uint16_t>(c.model.nodes.size() - 1);
        }
        c.pose = {{{0, 0, 0}, {}},
                  {{0, 0, 0}, turn(.1f, .2f, 0)},
                  {{.1f, .2f, 1}, turn(0, .3f, .1f)}, // Sleeve
                  {{0, -1, 1.7f}, turn(.4f, 0, 0)},
                  {{0, -.4f, 3.4f}, turn(.2f, .1f, .3f)},
                  {{0, -.3f, 3.9f}, turn(-.5f, .2f, 0)}}; // Hand
        return c;
    }
    ArmTarget target(DVector shoulder, DVector pole)
    {
        return {1, 3, 4, shoulder, pole};
    }

    bool solve_reach()
    {
        auto c = chain();
        const auto before = c.pose;
        const float l1 = length(sub(before[3].position, before[1].position));
        const float l2 = length(sub(before[4].position, before[3].position));
        const DVector shoulder{1.2f, .8f, .4f}, pole{1, -1, 0};
        ArmSolve result;
        CHECK(solve_arms(c.model, std::span<const ArmTarget>(std::array{target(shoulder, pole)}), c.pose,
                         std::span<ArmSolve>(&result, 1)));
        CHECK(!result.slid && near(result.shoulder, shoulder) && near(c.pose[1].position, shoulder));
        CHECK(near(result.elbow, c.pose[3].position));
        // Authored segment lengths; wrist, hand and the untouched root keep their exact poses.
        CHECK(std::abs(length(sub(c.pose[3].position, c.pose[1].position)) - l1) < 1e-4f);
        CHECK(std::abs(length(sub(c.pose[4].position, c.pose[3].position)) - l2) < 1e-4f);
        CHECK(same(c.pose[0], before[0]) && same(c.pose[4], before[4]) && same(c.pose[5], before[5]));
        // Each segment moved rigidly: the next joint and the sleeve keep their local offsets.
        CHECK(near(local(c.pose[1], c.pose[3].position), local(before[1], before[3].position)));
        CHECK(near(local(c.pose[1], c.pose[2].position), local(before[1], before[2].position)));
        CHECK(near(local(c.pose[3], c.pose[4].position), local(before[3], before[4].position)));
        // The authored hinge maps onto the new bend plane.
        const auto r1 = compose_rotation(c.pose[1].rotation, conjugate(before[1].rotation));
        const auto old_hinge =
            cross(sub(before[3].position, before[1].position), sub(before[4].position, before[3].position));
        const auto new_hinge =
            cross(sub(c.pose[3].position, c.pose[1].position), sub(c.pose[4].position, c.pose[3].position));
        CHECK(dot(scaled(new_hinge, 1 / length(new_hinge)),
                  rotate_vector(r1, scaled(old_hinge, 1 / length(old_hinge)))) > .999f);
        return true;
    }

    bool solve_pole()
    {
        const DVector shoulder{0, 1, 0};
        std::array<DVector, 2> elbows{};
        for (std::size_t i = 0; i < 2; ++i)
        {
            auto c = chain();
            const DVector pole{i ? -1.f : 1.f, 0, 0};
            CHECK(solve_arms(c.model, std::span<const ArmTarget>(std::array{target(shoulder, pole)}), c.pose));
            // The elbow leaves the shoulder-wrist line toward the pole.
            const auto axis = sub(c.pose[4].position, shoulder);
            const auto offset = sub(c.pose[3].position, shoulder);
            const auto out = sub(offset, scaled(axis, dot(offset, axis) / dot(axis, axis)));
            CHECK(dot(out, pole) > .1f && std::abs(out.y) < .5f);
            elbows[i] = c.pose[3].position;
        }
        CHECK(elbows[0].x > 0 && elbows[1].x < 0);
        return true;
    }

    bool solve_clamp()
    {
        auto c = chain();
        const auto wrist = c.pose[4].position;
        const float l1 = length(sub(c.pose[3].position, c.pose[1].position));
        const float l2 = length(sub(c.pose[4].position, c.pose[3].position));
        // Out of reach: the shoulder slides toward the wrist, the arm nearly straight.
        ArmSolve result;
        const DVector far{0, 0, -100};
        CHECK(solve_arms(c.model, std::span<const ArmTarget>(std::array{target(far, {0, -1, 0})}), c.pose,
                         std::span<ArmSolve>(&result, 1)));
        CHECK(result.slid && std::abs(length(sub(wrist, result.shoulder)) - (l1 + l2) * .9999f) < 1e-3f);
        const auto toward = sub(far, wrist), slid = sub(result.shoulder, wrist);
        CHECK(dot(toward, slid) / (length(toward) * length(slid)) > .9999f);
        CHECK(same(c.pose[4], chain().pose[4]));
        // Too close: it slides away to the shortest span.
        c = chain();
        CHECK(solve_arms(c.model, std::span<const ArmTarget>(std::array{target(add(wrist, {0, 0, -.01f}), {0, -1, 0})}),
                         c.pose, std::span<ArmSolve>(&result, 1)));
        CHECK(result.slid && length(sub(wrist, result.shoulder)) >= std::abs(l1 - l2));
        CHECK(std::abs(length(sub(c.pose[3].position, c.pose[1].position)) - l1) < 1e-3f);
        return true;
    }

    bool solve_invalid()
    {
        auto c = chain();
        const auto before = c.pose;
        auto refuse = [&](ArmTarget t)
        {
            return !solve_arms(c.model, std::span<const ArmTarget>(std::array{t}), c.pose) &&
                   std::equal(c.pose.begin(), c.pose.end(), before.begin(), same);
        };
        CHECK(refuse({3, 1, 4, {0, 1, 0}, {0, -1, 0}}));  // Lower above upper.
        CHECK(refuse({1, 3, 2, {0, 1, 0}, {0, -1, 0}}));  // Wrist (the sleeve) not under the lower arm.
        CHECK(refuse({1, 3, 40, {0, 1, 0}, {0, -1, 0}})); // Out of range.
        CHECK(refuse(target({NAN, 0, 0}, {0, -1, 0})));
        CHECK(refuse(target({0, 1, 0}, {0, INFINITY, 0})));
        c.pose[3].position = c.pose[1].position; // Zero-length upper arm.
        const auto collapsed = c.pose;
        CHECK(!solve_arms(c.model, std::span<const ArmTarget>(std::array{target({0, 1, 0}, {0, -1, 0})}), c.pose) &&
              std::equal(c.pose.begin(), c.pose.end(), collapsed.begin(), same));
        // A failing second target leaves the first unapplied too.
        c = chain();
        CHECK(!solve_arms(c.model,
                          std::span<const ArmTarget>(std::array{target({0, 1, 0}, {0, -1, 0}), ArmTarget{1, 3, 40}}),
                          c.pose) &&
              std::equal(c.pose.begin(), c.pose.end(), before.begin(), same));
        return true;
    }

    // A first-person weapon in the original rig layout (identity rotations, local offsets):
    // right arm holding a gun, a left arm framed out behind the camera, a grenade below the view
    // and a sight in view.
    struct Weapon
    {
        std::shared_ptr<Model> model = std::make_shared<Model>();
    };
    void add_node(Model& m, const char* name, int parent, DVector at, bool mesh)
    {
        ModelNode node;
        node.name = name;
        node.parent = parent;
        node.index = static_cast<std::uint16_t>(m.nodes.size());
        m.nodes.push_back(node);
        for (auto& clip : m.animations)
            clip.nodes.push_back({{{at, {}}}, {}, {}, {}});
        if (!mesh)
            return;
        const auto first = static_cast<std::uint16_t>(m.vertices.size());
        for (int i = 0; i < 8; ++i)
            m.vertices.push_back({{i & 1 ? .1f : -.1f, i & 2 ? .1f : -.1f, i & 4 ? .1f : -.1f},
                                  {0, 1, 0},
                                  static_cast<std::uint8_t>(node.index),
                                  {}});
        for (std::uint16_t f : {0, 4})
            m.faces.push_back({{},
                               {static_cast<std::uint16_t>(first + f), static_cast<std::uint16_t>(first + f + 1),
                                static_cast<std::uint16_t>(first + f + 2)},
                               {0, 1, 0}});
    }
    Weapon weapon(bool right_arm = true)
    {
        Weapon w;
        auto& m = *w.model;
        for (const char* name : {"static_model", "alt_fire"})
        {
            m.animations.push_back({});
            m.animations.back().name = name;
            m.animations.back().keys.push_back({});
        }
        add_node(m, "null1", -1, {0, 0, 0}, false);
        if (right_arm)
        {
            add_node(m, "LU_ARM2", 0, {.44f, .4f, -3.9f}, true);
            add_node(m, "ll_arm2", 1, {0, -.5f, 1.3f}, true);
            add_node(m, "d_null1", 2, {-.4f, -.1f, 1.6f}, false);
            add_node(m, "hand", 3, {0, -.2f, .1f}, true);
            add_node(m, "gun", 4, {0, .1f, .8f}, true);
        }
        const auto left = static_cast<int>(m.nodes.size());
        add_node(m, "lu_arm3", 0, {-1.9f, .4f, -4.4f}, true);
        add_node(m, "ll_arm3", left, {0, -1.2f, -.9f}, true);
        add_node(m, "d_null19", left + 1, {0, -1.3f, -.9f}, false);
        add_node(m, "cube6", left + 2, {0, -.1f, 0}, true);
        add_node(m, "can_6", 0, {-1.5f, -2.6f, -.5f}, true);
        add_node(m, "sight", 0, {0, .3f, 1}, true);
        // alt_fire brings the left wrist up in front of the camera.
        m.animations[1].nodes[left + 1].frames[0].position = {.5f, .5f, 1.2f};
        m.animations[1].nodes[left + 2].frames[0].position = {.6f, -.2f, 3.4f};
        m.min = {-2, -3, -5};
        m.max = {1, 1, 2};
        m.main_vertices = static_cast<std::uint32_t>(m.vertices.size());
        return w;
    }
    const DVector original_offset{.6f, -.6f, 1.9f};

    bool rig()
    {
        auto w = weapon();
        ViewArmRig r;
        CHECK(rig_view_arms(*w.model, "Models\\Weapons\\Beretta_pv.abc", false, r));
        CHECK(r.chains.size() == 2 && r.held == 0 && r.grip == ViewGrip::Hand);
        CHECK(r.chains[0].upper == 1 && r.chains[0].lower == 2 && r.chains[0].wrist == 3 && r.chains[0].right);
        CHECK(!r.chains[1].right);
        // The palm: the hand mesh's own rest vertices, not the gun below it.
        std::vector<NodePose> pose;
        CHECK(sample_pose(*w.model, 0, 0, pose) && near(r.anchor, pose[4].position));
        // The left-hand model holds the left chain.
        CHECK(rig_view_arms(*w.model, "lh_beretta_pv.abc", true, r) && r.held == 1 && r.grip == ViewGrip::Hand);
        // No chain on the model's side: an audited two-handed grip, else the bounds centre.
        w = weapon(false);
        CHECK(rig_view_arms(*w.model, "Models\\Weapons\\M16_pv.abc", false, r));
        CHECK(r.held < 0 && r.chains.size() == 1 && r.grip == ViewGrip::Audited && near(r.anchor, {0, -.39f, -.26f}));
        CHECK(rig_view_arms(*w.model, "models/weapons/unknown_pv.abc", false, r));
        CHECK(r.grip == ViewGrip::Bounds && near(r.anchor, {-.5f, -1, -1.5f}));
        // A chain missing its wrist null is not a chain.
        w = weapon();
        w.model->nodes[3].name = "null9";
        CHECK(rig_view_arms(*w.model, "beretta_pv.abc", false, r) && r.chains.size() == 1 && r.held < 0);
        return true;
    }

    ViewArmPlacement placement()
    {
        ViewArmPlacement p;
        p.grip = {{100, 50, 20}, turn(.2f, 1.5707963f, .1f)};
        p.scale = 8;
        p.shoulders = {DVector{70, 60, 10}, DVector{75, 60, 30}};
        p.poles = {DVector{0, -1, -.5f}, DVector{0, -1, .5f}};
        return p;
    }

    bool placement_and_targets()
    {
        auto w = weapon();
        ViewArmRig r;
        ViewArmPose held;
        CHECK(rig_view_arms(*w.model, "beretta_pv.abc", false, r));
        const auto p = placement();
        CHECK(pose_view_arms(*w.model, {}, r, p, original_offset, held));
        // The anchor lands on the grip, the model turned to the aim.
        CHECK(near(add(held.model.position, rotate_vector(held.model.rotation, scaled(r.anchor, p.scale))),
                   p.grip.position, 1e-3f));
        CHECK(near(rotate_vector(held.model.rotation, {0, 0, 1}), rotate_vector(p.grip.rotation, {0, 0, 1})));
        // One target (the left arm is framed out), its shoulder and pole in model space.
        CHECK(held.targets.size() == 1 && held.targets[0].upper == 1 && held.targets[0].wrist == 3);
        CHECK(near(add(held.model.position, rotate_vector(held.model.rotation, scaled(held.targets[0].shoulder, 8))),
                   p.shoulders[1], 1e-3f));
        CHECK(near(rotate_vector(held.model.rotation, held.targets[0].pole), p.poles[1]));
        CHECK(!pose_view_arms(*w.model, {}, r, {p.grip, 0, p.shoulders, p.poles}, original_offset, held));
        return true;
    }

    bool hiding()
    {
        auto w = weapon();
        ViewArmRig r;
        ViewArmPose held;
        CHECK(rig_view_arms(*w.model, "beretta_pv.abc", false, r));
        CHECK(pose_view_arms(*w.model, {}, r, placement(), original_offset, held));
        // Framed out by the original camera: the left arm (behind it) and the grenade (below).
        const std::vector<std::uint8_t> rest{0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0};
        CHECK(held.hidden == rest);
        // The held arm is always drawn, even when the original framed it out.
        CHECK(pose_view_arms(*w.model, {}, r, placement(), {0, 10, 0}, held));
        CHECK(!held.hidden[1] && !held.hidden[2] && !held.hidden[4] && !held.hidden[5] && held.hidden[11]);
        // alt_fire raises the left hand into view: the arm returns, solved from the left shoulder.
        AnimationTrack alt;
        alt.animation = 1;
        CHECK(pose_view_arms(*w.model, alt, r, placement(), original_offset, held));
        CHECK(!held.hidden[6] && !held.hidden[9] && held.targets.size() == 2 && held.targets[1].upper == 6);
        return true;
    }

    bool model_view()
    {
        auto w = weapon();
        const std::shared_ptr<const Model> shared = w.model;
        Resources resources;
        std::string error;
        ViewArmRig r;
        ViewArmPose held;
        CHECK(rig_view_arms(*shared, "beretta_pv.abc", false, r) &&
              pose_view_arms(*shared, {}, r, placement(), original_offset, held));
        ModelView scene;
        scene.gpu_skinning = true;
        CHECK(scene.add(shared, {}, "missing.dtx", held.model, {8, 8, 8}, true, true, {}, "arms", resources, error,
                        held.hidden, 1, false, held.targets));
        CHECK(scene.instances.size() == 1);
        // The palette carries the solved pose and the hidden flags.
        std::vector<NodePose> pose;
        std::vector<DVector> deformed;
        CHECK(sample_skin(*shared, 0, 0, pose, deformed) && solve_arms(*shared, held.targets, pose));
        const auto first = scene.instances[0].palette;
        for (std::size_t i = 0; i < pose.size(); ++i)
        {
            const auto& q = scene.palette[first + 2 * i];
            const auto& t = scene.palette[first + 2 * i + 1];
            CHECK(q[0] == pose[i].rotation.m_Vec.x && q[3] == pose[i].rotation.m_Spin);
            CHECK(t[0] == pose[i].position.x && t[2] == pose[i].position.z && t[3] == float(held.hidden[i]));
        }
        std::vector<NodePose> unsolved;
        CHECK(sample_pose(*shared, 0, 0, unsolved) && !near(unsolved[1].position, pose[1].position, .1f));
        // An invalid chain draws nothing and says so; so does the CPU path, which has no IK.
        scene.begin_frame();
        const std::array bad{ArmTarget{1, 2, 40}};
        CHECK(scene.add(shared, {}, "missing.dtx", held.model, {8, 8, 8}, true, true, {}, "bad", resources, error, {},
                        1, false, bad));
        CHECK(scene.instances.empty() && scene.entries.back().error == "Invalid arm IK chain");
        ModelView cpu;
        CHECK(cpu.add(shared, {}, "missing.dtx", held.model, {8, 8, 8}, true, true, {}, "cpu", resources, error, {}, 1,
                      false, held.targets));
        CHECK(cpu.entries.back().error == "Arm IK requires GPU skinning" && cpu.mesh.ranges.empty());
        return true;
    }

    // Every original view model at its definition's view offset: a grip (hand or audited), valid
    // chains, and arms that solve from shoulders 25-35 cm away at the chosen scale.
    bool catalog(const char* root)
    {
        Config config;
        config.data_dir = root;
        Resources resources;
        std::string error;
        CHECK(resources.mount_game(config, error));
        std::vector<std::pair<std::string, DVector>> models;
        for (int type = 0; type < 256; ++type)
        {
            const auto* data = weapon_definition(static_cast<DBYTE>(type));
            if (!data)
                continue;
            const DVector offset{data->m_vViewModelOffset.x, data->m_vViewModelOffset.y, data->m_vViewModelOffset.z};
            for (const char* path : {data->m_szViewModelFilename, data->m_szLeftViewModelFilename})
                if (path && resources.find(path) &&
                    std::none_of(models.begin(), models.end(), [&](const auto& m) { return m.first == path; }))
                    models.emplace_back(path, path == data->m_szLeftViewModelFilename &&
                                                      std::string_view(path).find("lh_") != std::string_view::npos
                                                  ? DVector{-offset.x, offset.y, offset.z}
                                                  : offset);
        }
        CHECK(models.size() >= 25);
        const float meters = 48 * view_model_meters;
        std::size_t audited = 0;
        for (const auto& [path, offset] : models)
        {
            std::vector<std::uint8_t> bytes;
            Model model;
            CHECK(resources.read(path, bytes, error) && read_model(bytes, model, error));
            const bool left = path.find("lh_") != std::string::npos;
            ViewArmRig r;
            CHECK(rig_view_arms(model, path, left, r));
            ViewArmPlacement p;
            p.grip = {{0, 0, 0}, {}};
            p.scale = meters;
            p.shoulders = {DVector{-.17f * 48, .25f * 48, -.3f * 48}, DVector{.17f * 48, .25f * 48, -.3f * 48}};
            p.poles = {DVector{-.45f, -1, -.35f}, DVector{.45f, -1, -.35f}};
            AnimationTrack rest;
            rest.animation = animation_index(model, "static_model");
            for (const char* clip : {"idle", "fire"})
                if (rest.animation == no_animation)
                    rest.animation = animation_index(model, clip);
            if (rest.animation == no_animation)
                rest.animation = 0;
            ViewArmPose held;
            CHECK(pose_view_arms(model, rest, r, p, offset, held));
            std::vector<NodePose> pose;
            std::vector<DVector> deformed;
            CHECK(sample_skin(model, rest.animation, 0, pose, deformed));
            for (const auto& chain : r.chains)
            {
                const float upper = length(sub(pose[chain.lower].position, pose[chain.upper].position));
                const float lower = length(sub(pose[chain.wrist].position, pose[chain.lower].position));
                CHECK(upper > .5f && upper < 2.5f && lower > .5f && lower < 2.5f);
            }
            std::vector<ArmSolve> solved(held.targets.size());
            CHECK(solve_arms(model, held.targets, pose, solved));
            std::cout << path << ": " << r.chains.size() << " chains, grip "
                      << (r.grip == ViewGrip::Hand      ? "hand"
                          : r.grip == ViewGrip::Audited ? "audited"
                                                        : "bounds")
                      << ", " << held.targets.size() << " solved, hidden:";
            std::vector<DVector> vertices;
            CHECK(sample_vertices(model, rest.animation, 0, vertices));
            for (std::size_t i = 0; i < held.hidden.size(); ++i)
                if (held.hidden[i])
                {
                    DVector lo{1e9f, 1e9f, 1e9f}, hi{-1e9f, -1e9f, -1e9f};
                    for (std::size_t v = 0; v < vertices.size(); ++v)
                        if (model.vertices[v].node == model.nodes[i].index)
                        {
                            const DVector e = add(vertices[v], offset);
                            lo = {std::min(lo.x, e.x), std::min(lo.y, e.y), std::min(lo.z, e.z)};
                            hi = {std::max(hi.x, e.x), std::max(hi.y, e.y), std::max(hi.z, e.z)};
                        }
                    std::cout << ' ' << model.nodes[i].name;
                    if (lo.x < 1e8f)
                        std::cout << '(' << lo.x << ',' << lo.y << ',' << lo.z << '/' << hi.x << ',' << hi.y << ','
                                  << hi.z << ')';
                }
            std::cout << '\n';
            CHECK(r.grip != ViewGrip::Bounds && (r.grip == ViewGrip::Hand) == (r.held >= 0));
            audited += r.grip == ViewGrip::Audited;
        }
        CHECK(audited == 11);
        return true;
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::string_view test = argv[1];
    const bool ok = test == "solve_reach"            ? solve_reach()
                    : test == "solve_pole"           ? solve_pole()
                    : test == "solve_clamp"          ? solve_clamp()
                    : test == "solve_invalid"        ? solve_invalid()
                    : test == "rig"                  ? rig()
                    : test == "placement"            ? placement_and_targets()
                    : test == "hiding"               ? hiding()
                    : test == "model_view"           ? model_view()
                    : test == "catalog" && argc == 3 ? catalog(argv[2])
                                                     : false;
    return ok ? 0 : 1;
}
