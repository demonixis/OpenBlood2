#include "client/network_world.h"
#include <algorithm>
#include <cmath>

namespace OpenBlood2
{
    NetworkWorldReceiver::StateResult NetworkWorldReceiver::receive(const Net::WorldState& state, std::string& error)
    {
        error.clear();
        if (!Net::valid_state(state))
        {
            error = "Invalid network pose packet";
            return StateResult::Refused;
        }
        if (!membership_sequence_ || state.world != world_.frame().world || state.membership != membership_sequence_ ||
            state.sequence < membership_sequence_ || state.time < membership_time_)
            return StateResult::Ignored;
        std::vector<const Net::ObjectMotion*> accepted;
        for (const auto& motion : state.objects)
        {
            const auto* object = world_.get(motion.id);
            if (!object || object->pose_revision != motion.pose_revision || motion.id.slot >= histories_.size())
                continue;
            const auto& history = histories_[motion.id.slot];
            if (history.generation != motion.id.generation || history.sequence >= state.sequence ||
                (!history.samples.empty() && history.samples.back().time > state.time))
                continue;
            if ((object->model && motion.animation.animation >= object->model->animations.size()) ||
                (object->sprite && motion.sprite_track.has_frame && !object->sprite->frames.empty() &&
                 motion.sprite_track.frame >= object->sprite->frames.size()))
            {
                error = "Network playback is outside the admitted asset";
                return StateResult::Refused;
            }
            accepted.push_back(&motion);
        }
        if (accepted.empty())
            return StateResult::Ignored;
        auto frame = world_.frame();
        ++frame.sequence;
        frame.time = std::max(frame.time, state.time);
        for (const auto* motion : accepted)
        {
            const auto found = std::lower_bound(frame.objects.begin(), frame.objects.end(), motion->id.slot,
                                                [](const auto& object, auto slot) { return object.id.slot < slot; });
            Net::apply_motion(*motion, *found);
        }
        if (!world_.apply(std::move(frame), error))
            return StateResult::Refused;
        for (const auto* motion : accepted)
        {
            auto& history = histories_[motion->id.slot];
            history.sequence = state.sequence;
            if (!history.samples.empty() && history.samples.back().time == state.time)
                history.samples.back() = {state.time, *motion};
            else
            {
                if (history.samples.size() == 8)
                    history.samples.erase(history.samples.begin());
                history.samples.push_back({state.time, *motion});
            }
        }
        return StateResult::Applied;
    }
    namespace
    {
        DVector mix(DVector a, DVector b, float t)
        {
            return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
        }
        DRotation mix(DRotation a, DRotation b, float t)
        {
            const double dot = double(a.m_Spin) * b.m_Spin + double(a.m_Vec.x) * b.m_Vec.x +
                               double(a.m_Vec.y) * b.m_Vec.y + double(a.m_Vec.z) * b.m_Vec.z;
            if (dot < 0)
            {
                b.m_Vec = {-b.m_Vec.x, -b.m_Vec.y, -b.m_Vec.z};
                b.m_Spin = -b.m_Spin;
            }
            DRotation q{mix(a.m_Vec, b.m_Vec, t), a.m_Spin + (b.m_Spin - a.m_Spin) * t};
            const auto norm = float(std::sqrt(double(q.m_Spin) * q.m_Spin + double(q.m_Vec.x) * q.m_Vec.x +
                                              double(q.m_Vec.y) * q.m_Vec.y + double(q.m_Vec.z) * q.m_Vec.z));
            q.m_Vec = {q.m_Vec.x / norm, q.m_Vec.y / norm, q.m_Vec.z / norm};
            q.m_Spin /= norm;
            return q;
        }
    } // namespace
    bool NetworkWorldReceiver::sample(double time, ObjectId predicted, ClientWorld& out, std::string& error) const
    {
        error.clear();
        if (!membership_sequence_ || !std::isfinite(time) || time < 0 || time > 1e12)
        {
            error = "Invalid interpolation clock/world";
            return false;
        }
        auto frame = world_.frame();
        for (auto& object : frame.objects)
        {
            if (object.id == predicted || (predicted && object.parent == predicted) ||
                object.id.slot >= histories_.size())
                continue;
            const auto& history = histories_[object.id.slot];
            if (history.generation != object.id.generation || history.samples.empty())
                continue;
            const auto& samples = history.samples;
            const auto upper = std::upper_bound(samples.begin(), samples.end(), time,
                                                [](double t, const Sample& s) { return t < s.time; });
            if (upper == samples.begin())
                Net::apply_motion(samples.front().motion, object);
            else if (upper == samples.end())
                Net::apply_motion(samples.back().motion, object);
            else
            {
                const auto& a = *(upper - 1);
                const auto& b = *upper;
                const auto t = float((time - a.time) / (b.time - a.time));
                auto value = a.motion;
                value.pose.position = mix(a.motion.pose.position, b.motion.pose.position, t);
                value.pose.rotation = mix(a.motion.pose.rotation, b.motion.pose.rotation, t);
                value.scale = mix(a.motion.scale, b.motion.scale, t);
                value.color = mix(a.motion.color, b.motion.color, t);
                value.alpha = a.motion.alpha + (b.motion.alpha - a.motion.alpha) * t;
                if (a.motion.animation.animation == b.motion.animation.animation &&
                    a.motion.animation.revision == b.motion.animation.revision &&
                    b.motion.animation.time >= a.motion.animation.time)
                    value.animation.time += (b.motion.animation.time - a.motion.animation.time) * t;
                Net::apply_motion(value, object);
            }
        }
        ClientWorld sampled;
        if (!sampled.begin(frame.world) || !sampled.apply(std::move(frame), error))
            return false;
        out = std::move(sampled);
        return true;
    }
} // namespace OpenBlood2
